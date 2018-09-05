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

#include <errno.h>
#include <string.h>
#include <glib.h>
#include <rpc/object.h>
#include <rpc/server.h>
#ifdef SYSTEMD_SUPPORT
#include <systemd/sd-daemon.h>
#endif
#include "internal.h"

static int rpc_server_accept(rpc_server_t, rpc_connection_t);
static void rpc_server_cleanup(rpc_server_t);
static bool rpc_server_valid(rpc_server_t);
static void * rpc_server_worker(void *);
static gboolean rpc_server_listen(void *);
static void server_queue_purge(rpc_server_t);

static void
rpc_server_cleanup(rpc_server_t server)
{

	g_main_context_invoke(server->rs_g_context,
	    (GSourceFunc)rpc_kill_main_loop, server->rs_g_loop);
        g_thread_join(server->rs_thread);

        g_main_loop_unref(server->rs_g_loop);
        g_main_context_unref(server->rs_g_context);
	g_queue_free(server->rs_calls);
}

static bool
rpc_server_valid(rpc_server_t server)
{

	g_mutex_lock(&server->rs_mtx);
	if (server->rs_closed) {
		g_mutex_unlock(&server->rs_mtx);
		return (false);
	}
	g_mutex_unlock(&server->rs_mtx);
	return (true);
}

static int
rpc_server_accept(rpc_server_t server, rpc_connection_t conn)
{

	g_mutex_lock(&server->rs_mtx);
	if (server->rs_closed) {
		server->rs_conn_refused++;
		conn->rco_server_released = true;
		g_mutex_unlock(&server->rs_mtx);
		return (-1);
	}
	server->rs_refcnt++; /* conn has reference */

	debugf("Server accepting connection %p", conn);
	conn->rco_rpc_context = server->rs_context;

	g_rw_lock_writer_lock(&server->rs_connections_rwlock);
	server->rs_connections = g_list_append(server->rs_connections, conn);
	rpc_connection_retain(conn);
	g_rw_lock_writer_unlock(&server->rs_connections_rwlock);

	server->rs_conn_made++;
	g_mutex_unlock(&server->rs_mtx);

	if (server->rs_event_handler != NULL)
		server->rs_event_handler(conn, RPC_SERVER_CLIENT_CONNECT);

	return (0);
}

/* undo accept */
void
rpc_server_disconnect(rpc_server_t server, rpc_connection_t conn)
{

	debugf("Disconnecting: %p, closed == %d\n", conn, server->rs_closed);

	g_mutex_lock(&server->rs_mtx);

	if (server->rs_closed) {
		g_mutex_unlock(&server->rs_mtx);
		return;
	}

	g_assert(conn->rco_aborted);

	g_rw_lock_writer_lock(&server->rs_connections_rwlock);
	server->rs_connections = g_list_remove(server->rs_connections, conn);
	g_rw_lock_writer_unlock(&server->rs_connections_rwlock);
	g_mutex_unlock(&server->rs_mtx);

	if (server->rs_event_handler != NULL)
		server->rs_event_handler(conn, RPC_SERVER_CLIENT_DISCONNECT);

	rpc_connection_release(conn);
}

GMainContext *
rpc_server_get_main_context(rpc_server_t server)
{

	return (server->rs_g_context);
}

static void *
rpc_server_worker(void *arg)
{
	rpc_server_t server = arg;

	g_main_context_push_thread_default(server->rs_g_context);
	g_main_loop_run(server->rs_g_loop);
	debugf("server thread exit");
	return (NULL);
}

static gboolean
rpc_server_listen(void *arg)
{
	rpc_server_t server = arg;
	const struct rpc_transport *transport;
	char *scheme;

	g_mutex_lock(&server->rs_mtx);

	scheme = g_uri_parse_scheme(server->rs_uri);
	transport = rpc_find_transport(scheme);
	g_free(scheme);

	if (transport == NULL) {
		debugf("No such transport %s", server->rs_uri);
		server->rs_error = rpc_error_create(ENXIO,
		    "Error: Transport Not Found", NULL);
		goto done;
	}

	debugf("selected transport %s", transport->name);
	server->rs_flags = transport->flags;
	if (transport->listen(server, server->rs_uri, server->rs_params) == 0)
	    server->rs_operational = true;

done:
	g_cond_signal(&server->rs_cv);
	g_mutex_unlock(&server->rs_mtx);
	return (false);
}

rpc_server_t
rpc_server_find(const char *uri, rpc_context_t context)
{
        rpc_server_t server;

	g_assert_nonnull(uri);
	g_assert_nonnull(context);

        g_rw_lock_reader_lock(&context->rcx_server_rwlock);
        for (guint i = 0; i < context->rcx_servers->len; i++) {
                server = g_ptr_array_index(context->rcx_servers, i);
		if (!strcmp(server->rs_uri, uri)) {
			g_rw_lock_reader_unlock(&context->rcx_server_rwlock);
			return (server);
		}
	}
	g_rw_lock_reader_unlock(&context->rcx_server_rwlock);
	return (NULL);
}

rpc_server_t
rpc_server_create(const char *uri, rpc_context_t context)
{

	return (rpc_server_create_ex(uri, context, NULL));
}

rpc_server_t
rpc_server_create_ex(const char *uri, rpc_context_t context, rpc_object_t params)
{
	rpc_server_t server;

	debugf("creating server %s", uri);

	server = g_malloc0(sizeof(*server));
	server->rs_uri = uri;
	server->rs_paused = true;
	server->rs_calls = g_queue_new();
	server->rs_context = context;
	server->rs_accept = rpc_server_accept;
	server->rs_valid = rpc_server_valid;
	server->rs_params = params;
	server->rs_g_context = g_main_context_new();
	server->rs_g_loop = g_main_loop_new(server->rs_g_context, false);
	server->rs_thread = g_thread_new("librpc server", rpc_server_worker,
	    server);
	g_cond_init(&server->rs_cv);
	g_mutex_init(&server->rs_mtx);
	g_mutex_init(&server->rs_calls_mtx);

	g_mutex_lock(&server->rs_mtx);
	g_main_context_invoke(server->rs_g_context, rpc_server_listen, server);

	while (server->rs_error == NULL && !server->rs_operational)
		g_cond_wait(&server->rs_cv, &server->rs_mtx);

	if (server->rs_error != NULL) {
                debugf("failed server create for %s with %s", uri,
		    rpc_error_get_message(server->rs_error));
		rpc_set_last_rpc_error(server->rs_error);
                rpc_server_cleanup(server);
		g_free(server);
                return (NULL);
	}

        g_rw_lock_writer_lock(&context->rcx_server_rwlock);
        g_ptr_array_add(context->rcx_servers, server);
        g_rw_lock_writer_unlock(&context->rcx_server_rwlock);

	server->rs_refcnt = 1;
	g_mutex_unlock(&server->rs_mtx);
	return (server);
}

void
rpc_server_broadcast_event(rpc_server_t server, const char *path,
    const char *interface, const char *name, rpc_object_t args)
{

	GList *item;

	g_rw_lock_reader_lock(&server->rs_connections_rwlock);
        if (server->rs_closed) {
		g_rw_lock_reader_unlock(&server->rs_connections_rwlock);
		return;
	}

	for (item = g_list_first(server->rs_connections); item;
	     item = item->next) {
		rpc_connection_t conn = item->data;
		rpc_connection_send_event(conn, path, interface, name, args);
	}
	g_rw_lock_reader_unlock(&server->rs_connections_rwlock);
}

void
rpc_server_set_event_handler(rpc_server_t server,
    rpc_server_ev_handler_t handler)
{

	if (server->rs_event_handler != NULL)
		Block_release(server->rs_event_handler);

	if (handler != NULL)
		server->rs_event_handler = Block_copy(handler);
}

int
rpc_server_dispatch(rpc_server_t server, struct rpc_call *call)
{
	int ret;

	g_mutex_lock(&server->rs_calls_mtx);

	if (server->rs_closed) {
		g_mutex_unlock(&server->rs_calls_mtx);
		call->rc_err =
		    rpc_error_create(ECONNRESET, "Server not active", NULL);
		return (-1);
	}

	if (server->rs_paused || !g_queue_is_empty(server->rs_calls))  {
		g_queue_push_tail(server->rs_calls, call);
		g_mutex_unlock(&server->rs_calls_mtx);
		return (0);
	}

	ret = rpc_context_dispatch(server->rs_context, call);
	g_mutex_unlock(&server->rs_calls_mtx);
	return (ret);
}

/*called with rs_call_mtx held*/
static void
server_queue_purge(rpc_server_t server)
{
	struct rpc_call *icall;

	while (!g_queue_is_empty(server->rs_calls)) {
		icall = g_queue_pop_head(server->rs_calls);

		if (!server->rs_closed) {
			if (rpc_context_dispatch(server->rs_context,
			    icall) == 0)
				continue;
		} else {
			icall->rc_err = rpc_error_create(ECONNRESET,
			    "Server not active", NULL);
		}

		if (icall->rc_err != NULL) {
			rpc_function_error(icall,
			    rpc_error_get_code(icall->rc_err),
			    rpc_error_get_message(icall->rc_err));
		}

		rpc_connection_close_inbound_call(icall);
	}
}


void
rpc_server_resume(rpc_server_t server)
{

	g_mutex_lock(&server->rs_calls_mtx);

	if (server->rs_closed) {
		g_mutex_unlock(&server->rs_calls_mtx);
		return;
	}

	server->rs_paused = false;
	server_queue_purge(server);
	g_mutex_unlock(&server->rs_calls_mtx);
}

void
rpc_server_pause(rpc_server_t server)
{

	g_mutex_lock(&server->rs_calls_mtx);
	if (server->rs_closed) {
		g_mutex_unlock(&server->rs_calls_mtx);
		return;
	}

	server->rs_paused = true;
	g_mutex_unlock(&server->rs_calls_mtx);
}

void
rpc_server_release(rpc_server_t server)
{

	g_mutex_lock(&server->rs_mtx);
	if (server->rs_closed && server->rs_refcnt == 1) {
		g_rw_lock_writer_lock(&server->rs_context->rcx_server_rwlock);
		g_ptr_array_remove(server->rs_context->rcx_servers, server);
		g_rw_lock_writer_unlock(&server->rs_context->rcx_server_rwlock);

		g_mutex_unlock(&server->rs_mtx);
		debugf("Server closed m: %d r: %d, c: %d, a: %d",
			server->rs_conn_made, server->rs_conn_refused,
			server->rs_conn_closed,
			server->rs_conn_aborted);
		server->rs_refcnt = -1;
		g_free(server);
		return;
	}
	server->rs_refcnt--;
	g_mutex_unlock(&server->rs_mtx);
}

int
rpc_server_close(rpc_server_t server)
{
	struct rpc_connection *conn;
	GList *iter = NULL;
	int ret = 0;
	int deref;

	if (server->rs_teardown == NULL) {
		rpc_set_last_errorf(ENOTSUP, "Not supported by transport");
		return (-1);
	}
	g_mutex_lock(&server->rs_mtx);
	g_mutex_lock(&server->rs_calls_mtx);
	server->rs_closed = true;
	server_queue_purge(server);
	g_mutex_unlock(&server->rs_calls_mtx);
	g_mutex_unlock(&server->rs_mtx);

	/* stop listening. */
	if (!server->rs_threaded_teardown)
		rpc_server_cleanup(server);
	ret = server->rs_teardown(server);

	debugf("TORNDOWN");

	/* Drop all connections */
	if (server->rs_connections != NULL) {
		for (iter = server->rs_connections; iter != NULL; iter = iter->next) {
			conn = iter->data;
			deref = rpc_connection_close(conn);
			server->rs_conn_aborted++;

			if (deref == -1) /* disconnect must fail, deref here */
				rpc_connection_release(conn);
		}
	}
	if (server->rs_threaded_teardown) {
		if (server->rs_teardown_end != NULL && ret == 0)
			server->rs_teardown_end(server);
		rpc_server_cleanup(server);
	}

	g_list_free(server->rs_connections); /*abort cleanup frees connections*/
        rpc_server_release(server);

	return (ret);
}

#ifdef SYSTEMD_SUPPORT
int
rpc_server_sd_listen(rpc_context_t context, rpc_server_t **servers,
    rpc_object_t *rest)
{
	rpc_server_t server;
	rpc_object_t params;
	const char *uri;
	char *name;
	char **names;
	int nfds;
	int i;
	int n = 0;

	if (rest != NULL)
		*rest = rpc_dictionary_create();

	nfds = sd_listen_fds_with_names(1, &names);
	if (nfds < 0) {
		rpc_set_last_errorf(-nfds, "Cannot get listen fds: %s",
		    strerror(-nfds));
		return (-1);
	}

	*servers = g_malloc0(sizeof(rpc_server_t) * nfds);

	for (i = SD_LISTEN_FDS_START; i < nfds + SD_LISTEN_FDS_START; i++) {
		if (!sd_is_socket(i, AF_UNSPEC, SOCK_STREAM, 1))
			continue;

		name = names[i - SD_LISTEN_FDS_START];

		if (g_strcmp0(name, "librpc.socket") == 0 ||
		    g_strcmp0(name, "librpc") == 0)
			uri = "socket://";
		else if (g_strcmp0(name, "librpc.ws") == 0)
			uri = "ws://";
		else {
			if (rest != NULL)
				rpc_dictionary_set_fd(*rest, name, i);

			free(name);
			continue;
		}

		params = rpc_fd_create(i);
		server = rpc_server_create_ex(uri, context, params);
		if (server == NULL)
			continue;

		(*servers)[n++] = server;
	}

	free(names);
	return (n);
}
#endif
