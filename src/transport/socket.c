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
#include "../internal.h"

static GSocketAddress *socket_parse_uri(const char *);
static struct rpc_transport_connection *socket_connect(const char *, rpc_object_t);
static struct rpc_transport_server *socket_listen(const char *, rpc_object_t);
static void *socket_reader(void *);

struct rpc_transport socket_transport = {
    .schemas = {"unix", "tcp", NULL},
    .connect = socket_connect,
    .listen = socket_listen
};

struct socket_server
{
    const char *	ss_uri;
    GSocketService *	ss_service;
    conn_handler_t	ss_conn_handler;

};

struct socket_connection
{
    const char *	sc_uri;
    GSocketConnection *	sc_conn;
    GSocketClient *	sc_client;
    GInputStream * 	sc_istream;
    GOutputStream *	sc_ostream;
    GThread *		sc_reader_thread;
    message_handler_t	sc_message_handler;
    close_handler_t	sc_close_handler;
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

static struct rpc_transport_connection *
socket_connect(const char *uri, rpc_object_t args)
{
	GError *err;
	GSocketAddress *addr;
	struct socket_connection *conn;

	addr = socket_parse_uri(uri);
	if (addr == NULL)
		return (NULL);

	conn = calloc(1, sizeof(*conn));
	conn->sc_uri = strdup(uri);
	conn->sc_client = g_socket_client_new();
	conn->sc_conn = g_socket_client_connect(conn->sc_client,
	    G_SOCKET_CONNECTABLE(&addr), NULL, &err);
	conn->sc_istream = g_io_stream_get_input_stream(
	    G_IO_STREAM(conn->sc_conn));
	conn->sc_ostream = g_io_stream_get_output_stream(
	    G_IO_STREAM(conn->sc_conn));
}

static struct rpc_transport_server *
socket_listen(const char *uri, rpc_object_t args)
{
	GError *err;
	GSocketAddress *addr;
	struct socket_server *server;

	addr = socket_parse_uri(uri);
	if (addr == NULL)
		return (NULL);

	server = calloc(1, sizeof(*server));
	server->ss_uri = strdup(uri);
	server->ss_service = g_socket_service_new();

	g_socket_listener_add_address(G_SOCKET_LISTENER(server->ss_service),
	    addr, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, NULL, NULL,
	    &err);
}

static int
socket_send_msg(void *arg, void *buf, size_t size, const int *fds, size_t nfds)
{
	struct socket_connection *conn = arg;
	GError *err;
	GSocket *sock = g_socket_connection_get_socket(conn->sc_conn);
	GSocketControlMessage *cmsg[2];
	GUnixFDList *fdlist;
	GOutputVector iov[2];
	uint32_t header[4] = { 0xdeadbeef, (uint32_t)size, 0, 0 };

	iov[0] = { .buffer = header, .size = sizeof(header) };
	iov[1] = { .buffer = buf, .size = size };

	cmsg[0] = g_unix_credentials_message_new();
	if (nfds > 0) {
		cmsg[1] = g_unix_fd_message_new_with_fd_list(
		    g_unix_fd_list_new_from_array(fds, (gint)nfds));
	}

	g_socket_send_message(sock, NULL, iov, 1, cmsg, nfds > 0 ? 2 : 1, 0,
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
    int **fds, size_t *nfds, struct rpc_credentials *creds)
{
	GError *err;
	GSocket *sock = g_socket_connection_get_socket(conn->sc_conn);
	GSocketControlMessage **cmsg;
	GUnixFDList *fdlist;
	GInputVector iov;
	uint32_t header[4];
	size_t length;
	gssize read;
	int ncmsg, flags, i;

	iov.buffer = header;
	iov.size = sizeof(header);

	g_socket_receive_message(sock, NULL, &iov, 1, &cmsg, &ncmsg, &flags,
	    NULL, &err);

	if (header[0] != 0xdeadbeef)
		return (-1);

	length = header[1];
	*frame = malloc(length);
	*size = length;

	for (i = 0; i < ncmsg; i++) {
		if (G_IS_UNIX_CREDENTIALS_MESSAGE(cmsg[i])) {
			GUnixCredentialsMessage *recv_creds;
			creds->rcc_remote_pid = recv_creds->native.pid;
			creds->rcc_remote_uid = recv_creds->native.uid;
			creds->rcc_remote_gid = recv_creds->native.gid;
			debugf("remote pid=%d, uid=%d, gid=%d",
			    creds->rcc_remote_pid, creds->rcc_remote_uid,
			    creds->rcc_remote_gid);
			continue;
		}

		if (G_IS_UNIX_FD_MESSAGE(cmsg[i])) {
			fds = g_unix_fd_message_steal_fds(cmsg[i], &nfds);
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

void
socket_abort(void *arg)
{
	struct socket_connection *conn = arg;
	GSocket *sock = g_socket_connection_get_socket(conn->sc_conn);

	g_socket_close(sock, NULL);
}

int
unix_get_fd(void *arg)
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
	int *fds;
	size_t len, nfds;
	int ret;

	for (;;) {
		ret = socket_recv_msg(conn, &frame, &len, &fds, &nfds, &creds);
		if (ret != 0)
			break;
	}

	conn->sc_close_handler();
	return (NULL);
}
