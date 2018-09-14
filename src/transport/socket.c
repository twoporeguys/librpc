/*+
 * Copyright 2015 Two Pore Guys, Inc.
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <gio/gio.h>
#ifndef _WIN32
#include <gio/gunixcredentialsmessage.h>
#include <gio/gunixfdmessage.h>
#include <gio/gunixsocketaddress.h>
#endif
#include <libsoup/soup.h>
#include "../linker_set.h"
#include "../internal.h"

#define SC_ABORT_TIMEOUT 30

static GSocketAddress *socket_parse_uri(const char *);
static int socket_connect(struct rpc_connection *, const char *, rpc_object_t);
static int socket_listen(struct rpc_server *, const char *, rpc_object_t);
static int socket_send_msg(void *, const void *, size_t, const int *, size_t);
static int socket_teardown(struct rpc_server *);
static int socket_abort(void *);
static int socket_get_fd(void *);
static void socket_release(void *);
static void *socket_reader(void *);
static gboolean socket_abort_timeout(gpointer user_data);
static bool socket_supports_fd_passing(struct rpc_connection *);

static const struct rpc_transport socket_transport = {
	.name = "socket",
	.schemas = {"unix", "tcp", "socket", NULL},
	.connect = socket_connect,
	.listen = socket_listen,
	.is_fd_passing = socket_supports_fd_passing,
	.flags = RPC_TRANSPORT_FD_PASSING | RPC_TRANSPORT_CREDENTIALS
};

struct socket_server
{
	char *				ss_uri;
	struct rpc_server *		ss_server;
	GSocketListener *		ss_listener;
	GCancellable *			ss_cancellable;
	GMutex 				ss_mtx;
	bool				ss_outstanding_accept;
};

struct socket_connection
{
	char *				sc_uri;
	GSocketConnection *		sc_conn;
	GSocket *			sc_socket;
	GThread *			sc_reader_thread;
	struct rpc_connection *		sc_parent;
	GMutex 				sc_abort_mtx;
	bool				sc_aborted;
	GCancellable *			sc_cancellable;
	GSource *			sc_abort_timeout;
	bool				sc_creds_sent;
};

static GSocketAddress *
socket_parse_uri(const char *uri_string)
{
	GSocketAddress *addr = NULL;
	GResolver *resolver;
	GError *err = NULL;
	GList *addresses;
	SoupURI *uri;
	char *host;
	char *upath;

	uri = soup_uri_new(uri_string);
	if (uri == NULL) {
		rpc_set_last_errorf(EINVAL, "Cannot parse URI");
		return (NULL);
	}

	if (!g_strcmp0(uri->scheme, "socket")) {
		soup_uri_free(uri);
		rpc_set_last_errorf(EINVAL,
		    "socket:// may be used only with file descriptors");
		return (NULL);
	}

	if (!g_strcmp0(uri->scheme, "tcp")) {
		resolver = g_resolver_get_default();
		addresses = g_resolver_lookup_by_name(resolver, uri->host, NULL, &err);
		g_object_unref(resolver);

		if (addresses == NULL || addresses->data == NULL) {
			rpc_set_last_gerror(err);
			g_error_free(err);
			return (NULL);
		}

		host = g_inet_address_to_string(addresses->data);
		addr = g_inet_socket_address_new_from_string(host, uri->port);

		g_free(host);
		g_list_free_full(addresses, g_object_unref);
		goto done;
	}

#ifndef _WIN32
	if (!g_strcmp0(uri->scheme, "unix")) {
		if (uri->host == NULL || uri->path ==NULL)
			return NULL;

		if (strlen(uri->host) + strlen(uri->path) == 0)
			return (NULL);

		upath = g_strdup_printf("%s%s", uri->host, uri->path);
		addr = g_unix_socket_address_new(upath);
		g_free(upath);
		goto done;
	}
#endif

done:
	soup_uri_free(uri);
	return (addr);
}

static void
socket_accept(GObject *source __unused, GAsyncResult *result, void *data)
{
	struct socket_server *server = data;
	struct socket_connection *conn = NULL;
	char *remote_addr = NULL;
	GError *err = NULL;
	GSocketConnection *gconn;
	GSocketAddress *remote;
	rpc_connection_t rco = NULL;
	rpc_server_t srv = server->ss_server;

	gconn = g_socket_listener_accept_finish(server->ss_listener, result,
	    NULL, &err);
	if (err != NULL) {
		debugf("accept failed");
		g_error_free(err);
		if (srv->rs_valid(srv))
			goto done;

		return;
	}

	if (!g_socket_set_option(g_socket_connection_get_socket(gconn),
	    SOL_SOCKET, SO_PASSCRED, true, &err)) {
		g_error_free(err);
		if (srv->rs_valid(srv))
			goto done;

		return;
	}

	remote = g_socket_connection_get_remote_address(gconn, NULL);
	if (remote != NULL) {
		if (G_IS_INET_SOCKET_ADDRESS(remote)) {
			remote_addr = g_inet_address_to_string(
			    g_inet_socket_address_get_address(
			        G_INET_SOCKET_ADDRESS(remote)));
		}

		if (G_IS_UNIX_SOCKET_ADDRESS(remote))
			remote_addr = g_strdup("unix");
	}

	conn = g_malloc0(sizeof(*conn));

	debugf("new connection %p", conn);
	conn->sc_conn = gconn;
	conn->sc_socket = g_object_ref(g_socket_connection_get_socket(gconn));
	g_mutex_init(&conn->sc_abort_mtx);

	rco = rpc_connection_alloc(srv);
	rco->rco_send_msg = socket_send_msg;
	rco->rco_get_fd = socket_get_fd;
	rco->rco_arg = conn;
	conn->sc_parent = rco;
	rco->rco_release = socket_release;
	rco->rco_abort = socket_abort;
	rco->rco_endpoint_address = remote_addr;

	if (srv->rs_accept(srv, rco) == 0) {
		conn->sc_cancellable = g_cancellable_new ();
		conn->sc_reader_thread = g_thread_new("socket reader thread",
		    socket_reader, (gpointer)conn);
	} else {
		rpc_connection_close(rco); /* will rco_abort, rco_release */
		return;
	}
done:
	/* Schedule next accept if server isn't closing */
	g_mutex_lock(&server->ss_mtx);
	g_cancellable_reset (server->ss_cancellable);
	g_socket_listener_accept_async(server->ss_listener,
	    server->ss_cancellable, &socket_accept, data);
	server->ss_outstanding_accept = true;
	g_mutex_unlock(&server->ss_mtx);
}

int
socket_connect(struct rpc_connection *rco, const char *uri,
    rpc_object_t args __unused)
{
	GError *err = NULL;
	GSocket *sock = NULL;
	GSocketAddress *addr = NULL;
	struct socket_connection *conn;

	if (args != NULL && rpc_get_type(args) == RPC_TYPE_FD) {
		sock = g_socket_new_from_fd(rpc_fd_get_value(args), &err);
		if (sock == NULL) {
			rpc_set_last_gerror(err);
			g_error_free(err);
			return (-1);
		}
	} else {
		addr = socket_parse_uri(uri);
		if (addr == NULL)
			return (-1);

		sock = g_socket_new(g_socket_address_get_family(addr),
		    G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, &err);
		if (sock == NULL) {
			rpc_set_last_gerror(err);
			g_object_unref(addr);
			g_error_free(err);
			return (-1);
		}

		g_socket_connect(sock, addr, NULL, &err);
	}

	if (err != NULL) {
		rpc_set_last_gerror(err);
		g_object_unref(addr);
		g_object_unref(sock);
		g_error_free(err);
		return (-1);
	}

	if (!g_socket_set_option(sock, SOL_SOCKET, SO_PASSCRED, true, &err)) {
		rpc_set_last_gerror(err);
		g_object_unref(addr);
		g_object_unref(sock);
		g_error_free(err);
		return (-1);
	}

	conn = g_malloc0(sizeof(*conn));
	conn->sc_parent = rco;
	conn->sc_uri = strdup(uri);
	g_mutex_init(&conn->sc_abort_mtx);

	rco->rco_release = socket_release;
	rco->rco_abort = socket_abort;
	rco->rco_arg = conn;

	conn->sc_socket = sock;
	rco->rco_send_msg = socket_send_msg;
	rco->rco_get_fd = socket_get_fd;
	conn->sc_cancellable = g_cancellable_new ();
	conn->sc_reader_thread = g_thread_new("socket reader thread",
	    socket_reader, (gpointer)conn);

	g_object_unref(addr);
	return (0);
}

int
socket_listen(struct rpc_server *srv, const char *uri,
    rpc_object_t args)
{
	GError *err = NULL;
	GFile *file;
	GUnixSocketAddress *uaddr;
	GSocketAddress *addr = NULL;
	GSocket *sock = NULL;
	struct socket_server *server;

	if (args != NULL && rpc_get_type(args) == RPC_TYPE_FD) {
		sock = g_socket_new_from_fd(rpc_fd_get_value(args), &err);
		if (sock == NULL) {
			srv->rs_error = rpc_error_create(err->code,
			    err->message, NULL);
			g_error_free(err);
			return (-1);
		}
	} else {
		addr = socket_parse_uri(uri);
		if (addr == NULL) {
			srv->rs_error = rpc_error_create(ENXIO, 
			    "No Such Address", NULL);
			return (-1);
		}
	}

	server = g_malloc0(sizeof(*server));
	server->ss_server = srv;
	server->ss_uri = strdup(uri);
	server->ss_listener = g_socket_listener_new();

	srv->rs_teardown = socket_teardown;
	srv->rs_arg = server;
	g_mutex_init(&server->ss_mtx);

	/*
	 * If using Unix domain sockets, make sure there's no stale socket
	 * file on the filesystem.
	 */
	if (addr != NULL) {
		if (g_socket_address_get_family(addr) == G_SOCKET_FAMILY_UNIX) {
			uaddr = G_UNIX_SOCKET_ADDRESS (addr);
			file = 
			    g_file_new_for_path(g_unix_socket_address_get_path(uaddr));

			if (g_file_query_exists(file, NULL)) {
				g_file_delete(file, NULL, &err);
				if (err != NULL) {
					srv->rs_error = rpc_error_create(err->code,
					    err->message, NULL);
					g_object_unref(addr);
					g_error_free(err);
					g_free(server->ss_uri);
					g_free(server);
					return (-1);
				}
			}

			g_object_unref(file);
		}

		g_socket_listener_add_address(server->ss_listener, addr,
		    G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, NULL,
		    NULL, &err);

		g_object_unref(addr);
	}

	if (sock != NULL)
		g_socket_listener_add_socket(server->ss_listener, sock, NULL,
		    &err);

	if (err != NULL) {
		srv->rs_error = rpc_error_create(err->code, err->message, NULL);
		g_error_free(err);
		g_object_unref(server->ss_listener);
		g_free(server->ss_uri);
		g_free(server);
		return (-1);
	}

	/* Schedule first accept */
	server->ss_cancellable = g_cancellable_new ();

	g_mutex_lock(&server->ss_mtx);
	g_socket_listener_accept_async(server->ss_listener,
	    server->ss_cancellable, &socket_accept, server);
	server->ss_outstanding_accept = true;
	g_mutex_unlock(&server->ss_mtx);

	return (0);
}

static int
socket_send_msg(void *arg, const void *buf, size_t size, const int *fds,
    size_t nfds)
{
	struct socket_connection *conn = arg;
	GError *err = NULL;
	GSocketControlMessage *cmsg[2] = { NULL };
	GOutputVector iov[2];
	uint32_t header[4] = { 0xdeadbeef, (uint32_t)size, 0, 0 };
	size_t done = 0;
	ssize_t step;
	size_t tmp;
	int ncmsg = 0;
	int ret = 0;
	int i;

	debugf("sending frame: addr=%p, len=%zu, nfds=%zu", buf, size, nfds);

	iov[0] = (GOutputVector){ .buffer = header, .size = sizeof(header) };
	iov[1] = (GOutputVector){ .buffer = buf, .size = size };

#ifndef _WIN32
	if (g_unix_credentials_message_is_supported()) {
		if (!conn->sc_creds_sent) {
			cmsg[ncmsg++] = g_unix_credentials_message_new();
			conn->sc_creds_sent = true;
		}

		if (nfds > 0)
			cmsg[ncmsg++] = g_unix_fd_message_new_with_fd_list(
			    g_unix_fd_list_new_from_array(fds, (gint)nfds));
	}
#endif

	for (;;) {
		step = g_socket_send_message(conn->sc_socket, NULL, iov, 2,
		    cmsg, ncmsg, 0, NULL, &err);
		if (err != NULL) {
			conn->sc_parent->rco_error =
			    rpc_error_create_from_gerror(err);
			g_error_free(err);
			ret = -1;
			goto done;
		}

		if (step == 0) {
			conn->sc_parent->rco_error = rpc_error_create(
			    ECONNRESET, "Connection terminated", NULL);
			ret = -1;
			goto done;
		}

		done += step;

		if (done == size + sizeof(header))
			break;

		for (i = 0; i < 2; i++) {
			tmp = MIN((size_t)step, (size_t)iov[i].size);
			iov[i].size -= tmp;
			iov[i].buffer += tmp;
			step -= tmp;
		}
	}

done:
	for (i = 0; i < ncmsg; i++)
		g_object_unref(cmsg[i]);

	return (ret);
}

static int
socket_recv_msg(struct socket_connection *conn, void **frame, size_t *size,
    int **fds, size_t *nfds, struct rpc_credentials *creds)
{
	GError *err = NULL;
	GSocketControlMessage **cmsg = NULL;
	GCredentials *cr;
	GInputVector iov[2];
	uint32_t header[4];
	ssize_t step;
	size_t length = 0;
	size_t done = 0;
	size_t tmp;
	bool have_header = false;
	int ncmsg = 0, i;
	int nfds_i;

	*nfds = 0;
	iov[0] = (GInputVector){ .buffer = header, .size = sizeof(header) };
	iov[1] = (GInputVector){ .buffer = NULL, .size = 0 };

	for (;;) {
		step = g_socket_receive_message(conn->sc_socket, NULL, iov, 2,
		    have_header ? NULL : &cmsg, have_header ? NULL : &ncmsg,
		    0, conn->sc_cancellable, &err);
		if (err != NULL) {
			conn->sc_parent->rco_error =
			    rpc_error_create_from_gerror(err);
			g_error_free(err);
			return (-1);
		}


		if (step == 0) {
			conn->sc_parent->rco_error = rpc_error_create(
			    ECONNRESET, "Connection terminated", NULL);
			return (-1);
		}

		done += step;

		for (i = 0; i < 2; i++) {
			tmp = MIN((size_t)step, (size_t)iov[i].size);
			iov[i].size -= tmp;
			iov[i].buffer += tmp;
			step -= tmp;
		}

		if (!have_header && done >= sizeof(header)) {
			/* First vector should be fully exhausted by now */
			g_assert(iov[0].size == 0);

			/* Now we have read enough to decode the header */
			if (header[0] != 0xdeadbeef)
				return (-1);

			have_header = true;
			length = header[1];
			*size = length;
			*frame = g_malloc(length);
			iov[1].buffer = *frame + done - sizeof(header);
			iov[1].size = length - done + sizeof(header);
		}

		if (done == length + sizeof(header))
			break;
	}

#ifndef _WIN32
	for (i = 0; i < ncmsg; i++) {
		if (G_IS_UNIX_CREDENTIALS_MESSAGE(cmsg[i])) {
			cr = g_unix_credentials_message_get_credentials(
			    G_UNIX_CREDENTIALS_MESSAGE(cmsg[i]));
			creds->rcc_pid = g_credentials_get_unix_pid(cr, &err);
			creds->rcc_uid = g_credentials_get_unix_user(cr, &err);
			creds->rcc_gid = (gid_t)-1;
			debugf("remote pid=%d, uid=%d, gid=%d", creds->rcc_pid,
			    creds->rcc_uid, creds->rcc_gid);
			continue;
		}

		if (G_IS_UNIX_FD_MESSAGE(cmsg[i])) {
			*fds = g_unix_fd_message_steal_fds(
			    G_UNIX_FD_MESSAGE(cmsg[i]), &nfds_i);
			*nfds = (size_t)nfds_i;
			continue;
		}

		g_object_unref(cmsg[i]);
	}
#endif

	if (cmsg != NULL)
		g_free(cmsg);

	g_cancellable_reset(conn->sc_cancellable);
	return (0);
}

static int
socket_abort(void *arg)
{
	struct socket_connection *conn = arg;

	g_mutex_lock(&conn->sc_abort_mtx);
	if (!conn->sc_aborted) {
		conn->sc_aborted = true;
		g_mutex_unlock(&conn->sc_abort_mtx);

		g_socket_shutdown(conn->sc_socket, true, true, NULL);
		g_socket_close(conn->sc_socket, NULL);

		if (conn->sc_reader_thread) {
			conn->sc_abort_timeout =
			    g_timeout_source_new_seconds(SC_ABORT_TIMEOUT);
			g_source_set_callback(conn->sc_abort_timeout,
			    &socket_abort_timeout, conn, NULL);
			g_source_attach(conn->sc_abort_timeout,
			    conn->sc_parent->rco_main_context);
			g_thread_join(conn->sc_reader_thread);
			if (!g_source_is_destroyed(conn->sc_abort_timeout))
				g_source_destroy(conn->sc_abort_timeout);
		}
	} else
		g_mutex_unlock(&conn->sc_abort_mtx);
	return (0);
}

static gboolean
socket_abort_timeout(gpointer user_data)
{
	struct socket_connection *conn = user_data;

	if (g_source_is_destroyed(g_main_current_source()))
		return (false);

	g_assert(g_main_current_source() == conn->sc_abort_timeout);
	g_source_destroy(conn->sc_abort_timeout);
	g_cancellable_cancel (conn->sc_cancellable);
	return (false);
}

static int
socket_get_fd(void *arg)
{
	struct socket_connection *conn = arg;

	return (g_socket_get_fd(conn->sc_socket));
}

static void
socket_release(void *arg)
{
	struct socket_connection *conn = arg;

	if (conn->sc_conn)
		g_object_unref(conn->sc_conn);
	if (conn->sc_uri)
		g_free(conn->sc_uri);
	if (conn->sc_abort_timeout) {
		if (!g_source_is_destroyed(conn->sc_abort_timeout))
			g_source_destroy(conn->sc_abort_timeout);
		g_source_unref(conn->sc_abort_timeout);
	}
	g_free(conn);
}

static int
socket_teardown(struct rpc_server *srv)
{
	struct socket_server *socket_srv = srv->rs_arg;

	g_mutex_lock(&socket_srv->ss_mtx);
	if (socket_srv->ss_outstanding_accept)
            g_cancellable_cancel (socket_srv->ss_cancellable);
	g_socket_listener_close(socket_srv->ss_listener);
	g_object_unref(socket_srv->ss_listener);
	g_mutex_unlock(&socket_srv->ss_mtx);

	return (0);
}

static void *
socket_reader(void *arg)
{
	struct socket_connection *conn = arg;
	struct rpc_credentials creds;
	void *frame;
	int *fds;
	size_t len, nfds;

	for (;;) {
		if (socket_recv_msg(conn, &frame, &len, &fds, &nfds, &creds) != 0)
			break;

		if (conn->sc_parent->rco_recv_msg(conn->sc_parent, frame, len,
		    fds, nfds, &creds) != 0) {
			g_free(frame);
			break;
		}

		g_free(frame);
	}

	conn->sc_parent->rco_close(conn->sc_parent);
	return (NULL);
}

static bool
socket_supports_fd_passing(struct rpc_connection *rpc_conn)
{
	struct socket_connection *conn = rpc_conn->rco_arg;

	return (g_socket_get_family(conn->sc_socket) == G_SOCKET_FAMILY_UNIX);
}

DECLARE_TRANSPORT(socket_transport);
