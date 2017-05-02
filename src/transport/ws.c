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

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <libsoup/soup.h>

#include "../internal.h"

static struct rpc_transport_connection *ws_connect(const char *, rpc_object_t);
static struct rpc_transport_server *ws_listen(const char *, rpc_object_t);
static void ws_receive_message(SoupWebsocketConnection *, SoupWebsocketDataType,
    GBytes *, gpointer);
static int ws_send_message(void *, void *, size_t);
static void ws_process_connection(SoupServer *, SoupWebsocketConnection *,
    const char *, SoupClientContext *, gpointer);

struct rpc_transport ws_transport = {
    .schemas = {"ws", "wss", NULL},
    .connect = ws_connect,
    .listen = ws_listen
};

struct ws_connection
{
	GSocketClient *			wc_client;
	GSocketConnection *		wc_conn;
	SoupWebsocketConnection *	wc_ws;
};

struct ws_server
{
	SoupServer *			ws_server;
};

static struct rpc_transport_connection *
ws_connect(const char *uri_string, rpc_object_t args)
{
	GError *err;
	SoupURI *uri;
	struct ws_connection *conn;

	uri = soup_uri_new(uri_string);

	conn = calloc(1, sizeof(*conn));
	conn->wc_client = g_socket_client_new();
	conn->wc_conn = g_socket_client_connect_to_host(conn->wc_client,
	    uri->host, (guint16)uri->port, NULL, &err);
	conn->wc_ws = soup_websocket_connection_new(G_IO_STREAM(conn->wc_conn),
	    soup_uri_new(uri_string), SOUP_WEBSOCKET_CONNECTION_CLIENT, NULL,
	    NULL);

	g_signal_connect(conn->wc_ws, "message",
	    G_CALLBACK(ws_receive_message), NULL);
}

static struct rpc_transport_server *
ws_listen(const char *uri, rpc_object_t args)
{
	struct ws_server *server;

	server = calloc(1, sizeof(*server));
	server->ws_server = soup_server_new("");

	soup_server_add_websocket_handler(server->ws_server, "/ws", "", NULL,
	    G_CALLBACK(ws_process_connection), NULL, NULL);
}

static void
ws_process_connection(SoupServer *server, SoupWebsocketConnection *connection,
    const char *path, SoupClientContext *client, gpointer user_data)
{
	struct ws_connection *conn;

	conn = calloc(1, sizeof(*conn));
	conn->wc_ws = connection;

	g_signal_connect(conn->wc_ws, "message",
	    G_CALLBACK(ws_receive_message), NULL);
}

static void
ws_receive_message(SoupWebsocketConnection *ws, SoupWebsocketDataType type,
    GBytes *message, gpointer user_data)
{

}

static int
ws_send_message(void *arg, void *buf, size_t len)
{
	struct ws_connection *conn = arg;

	soup_websocket_connection_send_binary(conn->wc_ws, buf, len);
}

int
ws_get_fd(ws_conn_t *conn)
{
	return conn->ws_fd;
}
