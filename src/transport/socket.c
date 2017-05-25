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
#include <gio/gio.h>
#include <gio/gunixcredentialsmessage.h>
#include <gio/gunixfdmessage.h>
#include <gio/gunixfdlist.h>
#include <gio/gunixsocketaddress.h>
#include <libsoup/soup.h>
#include "../linker_set.h"
#include "../internal.h"

static GSocketAddress *socket_parse_uri(const char *);
static int socket_connect(struct rpc_connection *, const char *, rpc_object_t);
static int socket_listen(struct rpc_server *, const char *, rpc_object_t);
static int socket_send_msg(void *, void *, size_t, const int *, size_t);
static int socket_abort(void *);
static int socket_get_fd(void *);
static void *socket_reader(void *);

struct rpc_transport socket_transport = {
	.name = "socket",
	.schemas = {"unix", "tcp", NULL},
	.connect = socket_connect,
	.listen = socket_listen
};

struct socket_server
{
	const char *			ss_uri;
	struct rpc_server *		ss_server;
	GSocketListener *		ss_listener;
};

struct socket_connection
{
	const char *			sc_uri;
	GSocketConnection *		sc_conn;
	GSocketClient *			sc_client;
	GInputStream * 			sc_istream;
	GOutputStream *			sc_ostream;
	GThread *			sc_reader_thread;
	struct rpc_connection *		sc_parent;
};

static GSocketAddress *
socket_parse_uri(const char *uri_string)
{
	GSocketAddress *addr;
	SoupURI *uri;

	uri = soup_uri_new(uri_string);

	if (!g_strcmp0(uri->scheme, "tcp")) {
		addr = g_inet_socket_address_new_from_string(uri->host,
		    uri->port);
		return (addr);
	}

	if (!g_strcmp0(uri->scheme, "unix")) {
		addr = g_unix_socket_address_new(uri->path);
		return (addr);
	}

	return (NULL);
}

static void
socket_accept(GObject *source, GAsyncResult *result, void *data)
{
	struct socket_server *server = data;
	struct socket_connection *conn;
	GError *err = NULL;
	GSocketConnection *gconn;
	rpc_connection_t rco;
	rpc_server_t srv = server->ss_server;

	gconn = g_socket_listener_accept_finish(server->ss_listener, result,
	    NULL, &err);
	if (err != NULL)
		goto done;

	debugf("new connection");

	conn = g_malloc0(sizeof(*conn));
	conn->sc_client = g_socket_client_new();
	conn->sc_conn = gconn;
	conn->sc_istream = g_io_stream_get_input_stream(G_IO_STREAM(gconn));
	conn->sc_ostream = g_io_stream_get_output_stream(G_IO_STREAM(gconn));

	rco = rpc_connection_alloc(srv);
	rco->rco_send_msg = &socket_send_msg;
	rco->rco_abort = &socket_abort;
	rco->rco_get_fd = &socket_get_fd;
	rco->rco_arg = conn;
	conn->sc_parent = rco;
	srv->rs_accept(srv, rco);

	conn->sc_reader_thread = g_thread_new("socket reader thread",
	    socket_reader, (gpointer)conn);

done:
	/* Schedule next accept */
	g_socket_listener_accept_async(server->ss_listener,  NULL,
	    &socket_accept, data);
}

int
socket_connect(struct rpc_connection *rco, const char *uri, rpc_object_t args)
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
	conn->sc_conn = g_socket_client_connect(conn->sc_client,
	    G_SOCKET_CONNECTABLE(addr), NULL, &err);
	conn->sc_istream = g_io_stream_get_input_stream(
	    G_IO_STREAM(conn->sc_conn));
	conn->sc_ostream = g_io_stream_get_output_stream(
	    G_IO_STREAM(conn->sc_conn));
	rco->rco_send_msg = &socket_send_msg;
	rco->rco_abort = &socket_abort;
	rco->rco_get_fd = &socket_get_fd;
	rco->rco_arg = conn;
	conn->sc_reader_thread = g_thread_new("socket reader thread",
	    socket_reader, (gpointer)conn);

	return (0);
}

int
socket_listen(struct rpc_server *srv, const char *uri, rpc_object_t args)
{
	GError *err = NULL;
	GSocketAddress *addr;
	struct socket_server *server;

	addr = socket_parse_uri(uri);
	if (addr == NULL)
		return (-1);

	server = g_malloc0(sizeof(*server));
	server->ss_server = srv;
	server->ss_uri = strdup(uri);
	server->ss_listener = g_socket_listener_new();

	g_socket_listener_add_address(server->ss_listener, addr,
	    G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, NULL, NULL, &err);
	if (err != NULL) {
		g_free(server);
	}

	/* Schedule first accept */
	g_socket_listener_accept_async(server->ss_listener,
	    NULL, &socket_accept, server);

	return (0);
}

static int
socket_send_msg(void *arg, void *buf, size_t size, const int *fds, size_t nfds)
{
	struct socket_connection *conn = arg;
	GError *err = NULL;
	GSocket *sock = g_socket_connection_get_socket(conn->sc_conn);
	GSocketControlMessage *cmsg[2];
	GUnixFDList *fdlist;
	GOutputVector iov[2];
	uint32_t header[4] = { 0xdeadbeef, (uint32_t)size, 0, 0 };
	int ncmsg = 0;

	debugf("sending frame: addr=%p, len=%zu, nfds=%zu", buf, size, nfds);

	iov[0] = (GOutputVector){ .buffer = header, .size = sizeof(header) };
	iov[1] = (GOutputVector){ .buffer = buf, .size = size };

	if (g_unix_credentials_message_is_supported()) {
		cmsg[ncmsg++] = g_unix_credentials_message_new();
		if (nfds > 0) {
			cmsg[ncmsg++] = g_unix_fd_message_new_with_fd_list(
			    g_unix_fd_list_new_from_array(fds, (gint)nfds));
		}
	}

	g_socket_send_message(sock, NULL, iov, 1, cmsg, ncmsg, 0,
	    NULL, &err);
	if (err != NULL) {
		g_error_free(err);
		return (-1);
	}

	g_output_stream_write(conn->sc_ostream, buf, size, NULL, &err);
	if (err != NULL) {
		g_error_free(err);
		return (-1);
	}

	return (0);
}

static int
socket_recv_msg(struct socket_connection *conn, void **frame, size_t *size,
    int *fds, size_t *nfds, struct rpc_credentials *creds)
{
	GError *err = NULL;
	GSocket *sock = g_socket_connection_get_socket(conn->sc_conn);
	GSocketControlMessage **cmsg;
	GCredentials *cr;
	GUnixFDList *fdlist;
	GInputVector iov;
	uint32_t header[4];
	size_t length;
	gssize read;
	int ncmsg, flags = 0, i;

	iov.buffer = header;
	iov.size = sizeof(header);

	g_socket_receive_message(sock, NULL, &iov, 1, &cmsg, &ncmsg, &flags,
	    NULL, &err);
	if (err != NULL) {
		g_free(err);
		return (-1);
	}

	if (header[0] != 0xdeadbeef)
		return (-1);

	length = header[1];
	*frame = malloc(length);
	*size = length;

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
			fds = g_unix_fd_message_steal_fds(
			    G_UNIX_FD_MESSAGE(cmsg[i]), (gint *)nfds);
			continue;
		}
	}

	read = g_input_stream_read(conn->sc_istream, *frame, *size, NULL, &err);
	if (err != NULL) {
		g_free(err);
		free(frame);
		return (-1);
	}

	if (read < length) {
		/* Short read */
		free(frame);
		return (-1);
	}

	return (0);
}

static int
socket_abort(void *arg)
{
	struct socket_connection *conn = arg;
	GSocket *sock = g_socket_connection_get_socket(conn->sc_conn);

	g_socket_close(sock, NULL);
	return (0);
}

static int
socket_get_fd(void *arg)
{
	struct socket_connection *conn = arg;
	GSocket *sock = g_socket_connection_get_socket(conn->sc_conn);

	return (g_socket_get_fd(sock));
}

static void *
socket_reader(void *arg)
{
	struct socket_connection *conn = arg;
	struct rpc_credentials creds;
	void *frame;
	int fds[128];
	size_t len, nfds;

	for (;;) {
		if (socket_recv_msg(conn, &frame, &len, fds, &nfds, &creds) != 0)
			break;

		if (conn->sc_parent->rco_recv_msg(conn->sc_parent, frame, len,
		    fds, nfds, &creds) != 0)
			break;

	}

	conn->sc_parent->rco_close(conn->sc_parent);
	return (NULL);
}

DECLARE_TRANSPORT(socket_transport);
