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
#include <string.h>
#include <errno.h>
#include <glib.h>
#include <libsoup/soup.h>
#include "../linker_set.h"
#include "../internal.h"

#define	WS_MAX_MESSAGE_SIZE	(1024 * 2048) /* 2MB */

static gboolean ws_do_connect(gpointer user_data);
static int ws_connect(struct rpc_connection *, const char *, rpc_object_t);
static void ws_connect_done(GObject *, GAsyncResult *, gpointer);
static int ws_listen(struct rpc_server *, const char *, rpc_object_t);
static void ws_receive_message(SoupWebsocketConnection *, SoupWebsocketDataType,
    GBytes *, gpointer);
static int ws_send_message(void *, const void *, size_t, const int *, size_t);
static void ws_process_banner(SoupServer *, SoupMessage *, const char *,
    GHashTable *, SoupClientContext *, gpointer);
static void ws_process_connection(SoupServer *, SoupWebsocketConnection *,
    const char *, SoupClientContext *, gpointer);
static void ws_error(SoupWebsocketConnection *, GError *, gpointer);
static void ws_close(SoupWebsocketConnection *, gpointer);
static int ws_abort(void *);
static int ws_get_fd(void *);
static void ws_release(void *);
static int ws_teardown(struct rpc_server *);
static int ws_teardown_end(struct rpc_server *);
static gboolean done_waiting (gpointer user_data);

struct rpc_transport ws_transport = {
	.name = "websocket",
	.schemas = {"ws", "wss", NULL},
	.connect = ws_connect,
	.listen = ws_listen
};

struct ws_connection
{
    	GMutex				wc_mtx;
    	GCond				wc_cv;
    	GError *			wc_connect_err;
	GError *			wc_last_err;
	GSocketClient *			wc_client;
	GSocketConnection *		wc_conn;
    	SoupSession *			wc_session;
	SoupURI *			wc_uri;
	SoupWebsocketConnection *	wc_ws;
	struct rpc_connection *		wc_parent;
	GMutex				wc_abort_mtx;
	GCond				wc_abort_cv;
	bool				wc_aborted;
	bool				wc_closed;
	struct ws_server *		wc_server;
};

struct ws_server
{
	struct rpc_server *		ws_server;
	SoupServer *			ws_soupserver;
	SoupURI *			ws_uri;
	bool				ws_done;
	GMutex				ws_mtx;
	GCond				ws_cv;
	GMutex				ws_abort_mtx;
};

static gboolean
ws_do_connect(gpointer user_data)
{
	SoupMessage *msg;
	struct ws_connection *conn = user_data;

	conn->wc_session = soup_session_new_with_options(
	    SOUP_SESSION_USE_THREAD_CONTEXT, TRUE, NULL);
	msg = soup_message_new_from_uri(SOUP_METHOD_GET, conn->wc_uri);
	soup_session_websocket_connect_async(conn->wc_session, msg, NULL, NULL,
	    NULL, &ws_connect_done, conn);

	return (false);
}

static int
ws_connect(struct rpc_connection *rco, const char *uri_string,
    rpc_object_t args __unused)
{
	struct ws_connection *conn = NULL;

	conn = g_malloc0(sizeof(*conn));
	g_mutex_init(&conn->wc_mtx);
	g_cond_init(&conn->wc_cv);
	g_mutex_init(&conn->wc_abort_mtx);
	g_cond_init(&conn->wc_abort_cv);
	conn->wc_uri = soup_uri_new(uri_string);
	conn->wc_parent = rco;

	g_main_context_invoke(rco->rco_main_context, ws_do_connect, conn);
	g_mutex_lock(&conn->wc_mtx);
	while (conn->wc_connect_err == NULL && conn->wc_ws == NULL)
		g_cond_wait(&conn->wc_cv, &conn->wc_mtx);
	g_mutex_unlock(&conn->wc_mtx);

	if (conn->wc_connect_err != NULL)
		goto err;

	return (0);
err:
	rpc_set_last_gerror(conn->wc_connect_err);
	ws_release(conn);
	return (-1);
}

static void
ws_connect_done(GObject *obj, GAsyncResult *res, gpointer user_data)
{
	GError *err = NULL;
	SoupWebsocketConnection *ws;
	struct ws_connection *conn = user_data;
	struct rpc_connection *rco = conn->wc_parent;

	ws = soup_session_websocket_connect_finish(SOUP_SESSION(obj),
	    res, &err);
	if (err != NULL) {
		g_mutex_lock(&conn->wc_mtx);
		conn->wc_connect_err = err;
		g_cond_signal(&conn->wc_cv);
		g_mutex_unlock(&conn->wc_mtx);
		return;
	}

	soup_websocket_connection_set_max_incoming_payload_size(ws,
	    WS_MAX_MESSAGE_SIZE);

	rco->rco_send_msg = ws_send_message;
	rco->rco_abort = ws_abort;
	rco->rco_get_fd = ws_get_fd;
	rco->rco_arg = conn;
	rco->rco_release = ws_release;

	g_mutex_lock(&conn->wc_mtx);
	conn->wc_ws = ws;
	g_signal_connect(conn->wc_ws, "closed", G_CALLBACK(ws_close), conn);
	g_signal_connect(conn->wc_ws, "message", G_CALLBACK(ws_receive_message),
	    conn);
	g_cond_broadcast(&conn->wc_cv);
	g_mutex_unlock(&conn->wc_mtx);
}

static int
ws_listen(struct rpc_server *srv, const char *uri_str,
    rpc_object_t args __unused)
{
	GError *err = NULL;
	GSocketAddress *addr = NULL;
	SoupURI *uri;
	struct ws_server *server;
	int ret = 0;

	uri = soup_uri_new(uri_str);
        if (uri != NULL)
		addr = g_inet_socket_address_new_from_string(uri->host,
	            uri->port);

	if (addr == NULL) {
                srv->rs_error = rpc_error_create(ENXIO, "No such address", NULL);
		if (uri != NULL)
			soup_uri_free(uri);

                return(-1);
	}

	server = calloc(1, sizeof(*server));
	server->ws_uri = uri;
	server->ws_server = srv;
	g_mutex_init(&server->ws_mtx);
	g_cond_init(&server->ws_cv);
	g_mutex_init(&server->ws_abort_mtx);
        srv->rs_teardown = &ws_teardown;
        srv->rs_teardown_end = &ws_teardown_end;
	srv->rs_threaded_teardown = true;
        srv->rs_arg = server;

	server->ws_soupserver = soup_server_new(
	    SOUP_SERVER_SERVER_HEADER, "librpc",
	    NULL);

	if (g_strcmp0(server->ws_uri->path, "/") != 0) {
		soup_server_add_handler(server->ws_soupserver, "/",
		    ws_process_banner, server, NULL);
	}

	soup_server_add_websocket_handler(server->ws_soupserver,
	    server->ws_uri->path, NULL, NULL, ws_process_connection, server,
	    NULL);

	soup_server_listen(server->ws_soupserver, addr, 0, &err);
	if (err != NULL) {
		srv->rs_error = rpc_error_create(err->code, err->message, NULL);
		g_error_free(err);
		soup_uri_free(server->ws_uri);
		g_object_unref(server->ws_soupserver);
		g_object_unref(addr);
		g_free(server);
		ret = -1;
	}

	g_object_unref(addr);
	return (ret);
}

static gboolean
done_waiting (gpointer user_data)
{
	struct ws_server *server = user_data;

	g_mutex_lock(&server->ws_mtx);
	server->ws_done = true;
        soup_server_disconnect (server->ws_soupserver);
	g_cond_broadcast(&server->ws_cv);
	g_mutex_unlock(&server->ws_mtx);

        return false;
}

static int
ws_teardown (struct rpc_server *srv)
{
	struct ws_server *server = srv->rs_arg;

	soup_server_remove_handler(server->ws_soupserver, "/");
	soup_server_remove_handler(server->ws_soupserver, server->ws_uri->path);
	return (0);
}


static int
ws_teardown_end (struct rpc_server *srv)
{
	struct ws_server *server = srv->rs_arg;
        GSource *source = g_idle_source_new ();

        g_source_set_priority (source, G_PRIORITY_LOW);
        g_source_set_callback (source, done_waiting, server, NULL);
        g_source_attach (source, srv->rs_g_context);
        g_source_unref (source);

	g_mutex_lock(&server->ws_mtx);
	while (!server->ws_done)
		g_cond_wait(&server->ws_cv, &server->ws_mtx);
	g_mutex_unlock(&server->ws_mtx);

	soup_uri_free(server->ws_uri);
	g_object_unref(server->ws_soupserver);

	g_free(server);
        return (0);
}


static void
ws_process_banner(SoupServer *ss __unused, SoupMessage *msg,
    const char *path __unused, GHashTable *query __unused,
    SoupClientContext *client __unused, gpointer user_data)
{
	struct ws_server *server = user_data;
	const char *resp = g_strdup_printf(
	    "<h1>Hello from librpc</h1>"
	    "<p>Please use WebSockets endpoint located at %s</p>",
	    server->ws_uri->path);

	soup_message_set_status(msg, 200);
	soup_message_body_append(msg->response_body, SOUP_MEMORY_STATIC, resp, strlen(resp));
}

static void
ws_process_connection(SoupServer *ss __unused,
    SoupWebsocketConnection *connection, const char *path __unused,
    SoupClientContext *client __unused, gpointer user_data)
{
	rpc_connection_t rco;
	struct ws_server *server = user_data;
	struct ws_connection *conn;

	debugf("new connection");

	conn = g_malloc0(sizeof(*conn));
	conn->wc_ws = connection;
	conn->wc_server = server;

	g_mutex_init(&conn->wc_abort_mtx);
	g_cond_init(&conn->wc_abort_cv);

	rco = rpc_connection_alloc(server->ws_server);
	rco->rco_send_msg = ws_send_message;
	rco->rco_abort = ws_abort;
	rco->rco_get_fd = ws_get_fd;
	rco->rco_arg = conn;
	conn->wc_parent = rco;
	rco->rco_release = ws_release;

	if (server->ws_server->rs_accept(server->ws_server, rco) != 0) {
		rpc_connection_close(rco);
		return;
	}

	g_object_ref(conn->wc_ws);
	g_signal_connect(conn->wc_ws, "closed", G_CALLBACK(ws_close), conn);
	g_signal_connect(conn->wc_ws, "error", G_CALLBACK(ws_error), conn);
	g_signal_connect(conn->wc_ws, "message",
	    G_CALLBACK(ws_receive_message), conn);
}

static void
ws_receive_message(SoupWebsocketConnection *ws __unused,
    SoupWebsocketDataType type __unused, GBytes *message, gpointer user_data)
{
	struct ws_connection *conn = user_data;
	const void *data;
	size_t len;

	data = g_bytes_get_data(message, &len);
	debugf("received frame: addr=%p, len=%zu", data, len);
	conn->wc_parent->rco_recv_msg(conn->wc_parent, data, len, NULL, 0, NULL);
}

static void
ws_error(SoupWebsocketConnection *ws __unused, GError *error, gpointer user_data)
{
	struct ws_connection *conn = user_data;

	debugf("err");
	conn->wc_last_err = g_error_copy(error);
}

static void
ws_close(SoupWebsocketConnection *ws __unused, gpointer user_data)
{
	struct ws_connection *conn = user_data;

	debugf("ws closed: ws conn=%p, rpc conn=%p", conn, conn->wc_parent);

	g_mutex_lock(&conn->wc_abort_mtx);
	conn->wc_aborted = true; /* prevent repeat ws_aborts after this */
	g_mutex_unlock(&conn->wc_abort_mtx);

	/* hold a reference to keep conn accessible after this abort-close*/
	rpc_connection_retain(conn->wc_parent);

	conn->wc_parent->rco_close(conn->wc_parent);

	if (conn->wc_last_err != NULL) {
		conn->wc_parent->rco_error =
		    rpc_error_create_from_gerror(conn->wc_last_err);
		g_error_free(conn->wc_last_err);
	}
	g_mutex_lock(&conn->wc_abort_mtx);
	conn->wc_closed = true;
	g_cond_broadcast(&conn->wc_abort_cv);
	g_mutex_unlock(&conn->wc_abort_mtx);
	rpc_connection_release(conn->wc_parent);
}

static int
ws_send_message(void *arg, const void *buf, size_t len,
    const int *fds __unused, size_t nfds __unused)
{
	struct ws_connection *conn = arg;

	if (soup_websocket_connection_get_state(conn->wc_ws) != SOUP_WEBSOCKET_STATE_OPEN)
		return (-1);

	soup_websocket_connection_send_binary(conn->wc_ws, buf, len);
	return (0);
}

static int
ws_abort(void *arg)
{
	struct ws_connection *conn = arg;

	debugf("ws abort %p", conn->wc_parent);
	g_mutex_lock(&conn->wc_abort_mtx);
	if (conn->wc_aborted || conn->wc_closed ||
	    soup_websocket_connection_get_state(conn->wc_ws) !=
	    SOUP_WEBSOCKET_STATE_OPEN) {
		g_mutex_unlock(&conn->wc_abort_mtx);
		return 0;
	}

	conn->wc_aborted = true;
	g_mutex_unlock(&conn->wc_abort_mtx);
	if (soup_websocket_connection_get_state(conn->wc_ws) !=
			SOUP_WEBSOCKET_STATE_OPEN)
		return (0);
	else
		soup_websocket_connection_close(conn->wc_ws, 1000, "Going away");

	g_mutex_lock(&conn->wc_abort_mtx);
	while (conn->wc_closed == false)
		g_cond_wait(&conn->wc_abort_cv, &conn->wc_abort_mtx);
	g_mutex_unlock(&conn->wc_abort_mtx);
	return (0);
}

static void
ws_release(void *arg)
{
	struct ws_connection *conn = arg;

	if (conn->wc_uri)
		soup_uri_free(conn->wc_uri);

	if (conn->wc_session)
		g_object_unref(conn->wc_session);

	if (conn->wc_ws)
		g_object_unref(conn->wc_ws);

	g_free(conn);
}

static int
ws_get_fd(void *arg)
{
	struct ws_connection *conn = arg;

	return (g_socket_get_fd(g_socket_connection_get_socket(conn->wc_conn)));
}

DECLARE_TRANSPORT(ws_transport);
