/*
 * Copyright 2015-2017 Two Pore Guys, Inc.
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

#include <glib.h>
#include <libsoup/soup.h>
#include <rpc/object.h>
#include <rpc/connection.h>
#include <rpc/client.h>
#include "internal.h"

struct ws_server
{
	SoupServer *		server;
};

struct ws_connection
{
	rpc_client_t 		client;
	rpc_connection_t 	conn;
	SoupWebsocketConnection	ws_conn;
};

static void
ws_receive_message(SoupWebsocketConnection *ws __unused,
    SoupWebsocketDataType type __unused, GBytes *message, gpointer user_data)
{
	struct ws_connection *conn = user_data;
	const void *data;
	size_t len;

	data = g_bytes_get_data(message, &len);

	if (rpc_connection_send_raw_message(conn->conn, data, len, NULL, 0) != 0) {

	}
}

static void
ws_error(SoupWebsocketConnection *ws __unused, GError *error,
    gpointer user_data)
{
	struct ws_connection *conn = user_data;

}

static void
ws_close(SoupWebsocketConnection *ws __unused, gpointer user_data)
{

}

static void
ws_process_connection(SoupServer *ss __unused,
    SoupWebsocketConnection *connection, const char *path,
    SoupClientContext *client __unused, gpointer user_data)
{
	struct ws_connection *wsconn;
	struct rpcd_service *service;

	service = rpcd_find_service(&path[1]);
	if (service == NULL) {
		soup_websocket_connection_close(connection,
		    SOUP_WEBSOCKET_CLOSE_BAD_DATA, "Service not found");
		return;
	}

	wsconn = g_malloc0(sizeof(*wsconn));
	wsconn->client

	g_object_ref(conn);
	g_signal_connect(conn->wc_ws, "closed", G_CALLBACK(ws_close), conn);
	g_signal_connect(conn->wc_ws, "error", G_CALLBACK(ws_error), conn);
	g_signal_connect(conn->wc_ws, "message",
	    G_CALLBACK(ws_receive_message), conn);
}

int
ws_start(void)
{
	struct ws_server *server;

	server = g_malloc0(sizeof(*server));
	server->server = soup_server_new(
	    SOUP_SERVER_SERVER_HEADER, "rpcd",
	    NULL);

	soup_server_add_websocket_handler(server->server, NULL, NULL, NULL,
	    ws_process_connection, server, NULL);


}
