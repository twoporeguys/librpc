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
#include <gio/gio.h>
#ifndef _WIN32
#include <gio/gunixcredentialsmessage.h>
#include <gio/gunixfdmessage.h>
#include <gio/gunixsocketaddress.h>
#endif
#include <libsoup/soup.h>
#include "../linker_set.h"
#include "../internal.h"

//static void *socket_parse_uri(const char *);
static GSocketAddress *socket_parse_uri(const char *);
static int socket_connect(struct rpc_connection *, const char *, rpc_object_t);
static int socket_listen(struct rpc_server *, const char *, rpc_object_t);
static int socket_send_msg(void *, void *, size_t, const int *, size_t);
static int socket_teardown(struct rpc_server *);
static int socket_abort(void *);
static int socket_get_fd(void *);
static void socket_release(void *);
static void *socket_reader(void *);

static const struct rpc_transport socket_transport = {
	.name = "socket",
	.schemas = {"unix", "tcp", NULL},
	.connect = socket_connect,
	.listen = socket_listen
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
	GSocketClient *			sc_client;
	GInputStream * 			sc_istream;
	GOutputStream *			sc_ostream;
	GThread *			sc_reader_thread;
	struct rpc_connection *		sc_parent;
	GMutex 				sc_abort_mtx;
	bool				sc_aborted;
};

static GSocketAddress *
socket_parse_uri(const char *uri_string)
{
	GSocketAddress *addr = NULL;
	SoupURI *uri;
	int len = 0;
	char *upath;

	uri = soup_uri_new(uri_string);
	if (uri == NULL)
	    return NULL;

	if (!g_strcmp0(uri->scheme, "tcp")) {
		addr = g_inet_socket_address_new_from_string(uri->host,
		    uri->port);
		goto done;
	}

#ifndef _WIN32
	if (!g_strcmp0(uri->scheme, "unix")) {
		if (uri->host == NULL || uri->path ==NULL)
			return NULL;
		len = strlen(uri->host) + strlen(uri->path);
		if (len == 0)
			return (NULL);
		upath = g_malloc0(len + 1);
		strcpy(upath, uri->host);
		strcat(upath, uri->path);

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
	GError *err = NULL;
	GSocketConnection *gconn;
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

	conn = g_malloc0(sizeof(*conn));

	debugf("new connection %p", conn);
	conn->sc_client = g_socket_client_new();
	conn->sc_conn = gconn;
	g_mutex_init(&conn->sc_abort_mtx);

	rco = rpc_connection_alloc(srv);
	rco->rco_send_msg = socket_send_msg;
	rco->rco_get_fd = socket_get_fd;
	rco->rco_arg = conn;
	conn->sc_parent = rco;
	rco->rco_release = socket_release;
	rco->rco_abort = socket_abort;

	if (srv->rs_accept(srv, rco) == 0) {
		conn->sc_istream =
		    g_io_stream_get_input_stream(G_IO_STREAM(gconn));
		conn->sc_ostream =
		    g_io_stream_get_output_stream(G_IO_STREAM(gconn));
		conn->sc_reader_thread = g_thread_new("socket reader thread",
		    socket_reader, (gpointer)conn);
	} else {
		rpc_connection_close(rco); /* will rco_abort, rco_release */
		return;
	}
done:
	/* Schedule next accept if server isn't closing*/
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
	GSocketAddress *addr;
	struct socket_connection *conn;

	addr = socket_parse_uri(uri);
	if (addr == NULL)
		return (-1);

	conn = g_malloc0(sizeof(*conn));
	conn->sc_parent = rco;
	conn->sc_uri = strdup(uri);
	conn->sc_client = g_socket_client_new();
	g_mutex_init(&conn->sc_abort_mtx);

	rco->rco_release = socket_release;
	rco->rco_abort = socket_abort;
	rco->rco_arg = conn;

	conn->sc_conn = g_socket_client_connect(conn->sc_client,
	    G_SOCKET_CONNECTABLE(addr), NULL, &err);
	if (err != NULL) {
		socket_release(conn);
		g_object_unref(addr);
		rpc_set_last_gerror(err);
		g_error_free(err);
		return (-1);
	}

	conn->sc_istream = g_io_stream_get_input_stream(
	    G_IO_STREAM(conn->sc_conn));
	conn->sc_ostream = g_io_stream_get_output_stream(
	    G_IO_STREAM(conn->sc_conn));
	rco->rco_send_msg = socket_send_msg;
	rco->rco_get_fd = socket_get_fd;
	conn->sc_reader_thread = g_thread_new("socket reader thread",
	    socket_reader, (gpointer)conn);

	g_object_unref(addr);
	return (0);
}

int
socket_listen(struct rpc_server *srv, const char *uri,
    rpc_object_t args __unused)
{
	GError *err = NULL;
	GFile *file;
	GSocketAddress *addr;
	GUnixSocketAddress *uaddr;
	struct socket_server *server;

	addr = socket_parse_uri(uri);
	if (addr == NULL) {
		srv->rs_error = rpc_error_create(ENXIO, "No Such Address", NULL);
		return (-1);
	}

	server = g_malloc0(sizeof(*server));
	server->ss_server = srv;
	server->ss_uri = strdup(uri);
	server->ss_listener = g_socket_listener_new();

	srv->rs_teardown = socket_teardown;
	srv->rs_arg = server;
	g_mutex_init(&server->ss_mtx);

#ifndef _WIN32
	/*
	 * If using Unix domain sockets, make sure there's no stale socket
	 * file on the filesystem.
	 */
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
	}
#endif

	g_socket_listener_add_address(server->ss_listener, addr,
	    G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, NULL, NULL, &err);
	if (err != NULL) {
		srv->rs_error = rpc_error_create(err->code, err->message, NULL);
		g_object_unref(addr);
		g_error_free(err);
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

	g_object_unref(addr);
	return (0);
}

static int
socket_send_msg(void *arg, void *buf, size_t size, const int *fds, size_t nfds)
{
	struct socket_connection *conn = arg;
	GError *err = NULL;
	GSocket *sock = g_socket_connection_get_socket(conn->sc_conn);
	GSocketControlMessage *cmsg[2] = { NULL };
	GOutputVector iov[2];
	uint32_t header[4] = { 0xdeadbeef, (uint32_t)size, 0, 0 };
	int ncmsg = 0;
	int ret = 0;

	debugf("sending frame: addr=%p, len=%zu, nfds=%zu", buf, size, nfds);

	iov[0] = (GOutputVector){ .buffer = header, .size = sizeof(header) };
	iov[1] = (GOutputVector){ .buffer = buf, .size = size };

#ifndef _WIN32
	if (g_unix_credentials_message_is_supported()) {
		cmsg[ncmsg++] = g_unix_credentials_message_new();
		if (nfds > 0) {
			cmsg[ncmsg++] = g_unix_fd_message_new_with_fd_list(
			    g_unix_fd_list_new_from_array(fds, (gint)nfds));
		}
	}
#endif

	g_socket_send_message(sock, NULL, iov, 1, cmsg, ncmsg, 0,
	    NULL, &err);
	if (err != NULL) {
		conn->sc_parent->rco_error =
			rpc_error_create_from_gerror(err);
		g_error_free(err);
		ret = -1;
		goto done;
	}

	if (!g_output_stream_write_all(conn->sc_ostream, buf, size, NULL, NULL, &err)) {
		conn->sc_parent->rco_error =
			rpc_error_create_from_gerror(err);
		g_error_free(err);
		ret = -1;
		goto done;
	}

done:
	for (int i = 0; i < ncmsg; i++)
		g_object_unref(cmsg[i]);

	return (ret);
}

static int
socket_recv_msg(struct socket_connection *conn, void **frame, size_t *size,
    int **fds, size_t *nfds, struct rpc_credentials *creds)
{
	GError *err = NULL;
	GSocket *sock = g_socket_connection_get_socket(conn->sc_conn);
	GSocketControlMessage **cmsg;
	GCredentials *cr;
	GInputVector iov;
	uint32_t header[4];
	size_t length;
	int ncmsg, flags = 0, i;

	iov.buffer = header;
	iov.size = sizeof(header);

	g_socket_receive_message(sock, NULL, &iov, 1, &cmsg, &ncmsg, &flags,
	    NULL, &err);
	if (err != NULL) {
		conn->sc_parent->rco_error =
			rpc_error_create_from_gerror(err);
		g_error_free(err);
		return (-1);
	}

	if (header[0] != 0xdeadbeef)
		return (-1);

	length = header[1];
	*frame = g_malloc(length);
	*size = length;

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
			    G_UNIX_FD_MESSAGE(cmsg[i]), (gint *)nfds);
			continue;
		}
	}
#endif

	if (!g_input_stream_read_all(conn->sc_istream, *frame, *size, NULL,
	   NULL, &err)) {
		conn->sc_parent->rco_error =
			rpc_error_create_from_gerror(err);
		g_error_free(err);
		free(*frame);
		return (-1);
	}

	return (0);
}

static int
socket_abort(void *arg)
{
	struct socket_connection *conn = arg;
	GSocket *sock = g_socket_connection_get_socket(conn->sc_conn);

	g_mutex_lock(&conn->sc_abort_mtx);
	if (!conn->sc_aborted) {
		conn->sc_aborted = true;
		g_mutex_unlock(&conn->sc_abort_mtx);
		g_socket_shutdown(sock, true, true, NULL);
		g_socket_close(sock, NULL);

		if (conn->sc_reader_thread) {
			g_thread_join(conn->sc_reader_thread);
		}
	} else
		g_mutex_unlock(&conn->sc_abort_mtx);
	return (0);
}

static int
socket_get_fd(void *arg)
{
	struct socket_connection *conn = arg;
	GSocket *sock = g_socket_connection_get_socket(conn->sc_conn);

	return (g_socket_get_fd(sock));
}

static void
socket_release(void *arg)
{
	struct socket_connection *conn = arg;

	if (conn->sc_conn)
		g_object_unref(conn->sc_conn);
	if (conn->sc_client)
		g_object_unref(conn->sc_client);
	if (conn->sc_uri)
		g_free(conn->sc_uri);
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

DECLARE_TRANSPORT(socket_transport);
