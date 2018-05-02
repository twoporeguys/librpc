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
#include "internal.h"

static int rpc_server_accept(rpc_server_t, rpc_connection_t);
static void rpc_server_cleanup(rpc_server_t);
static bool rpc_server_valid(rpc_server_t);
static void rpc_server_disconnect(rpc_server_t, rpc_connection_t);
static void * rpc_server_worker(void *);
static gboolean rpc_server_listen(void *);
static void server_queue_purge(rpc_server_t);

static void
rpc_server_cleanup(rpc_server_t server)
{

	if (server->rs_teardown_end == NULL)
	        g_main_context_invoke(server->rs_g_context,
        	    (GSourceFunc)rpc_kill_main_loop, server->rs_g_loop);
        g_thread_join(server->rs_thread);
	fprintf(stderr, "server thread JOINED\n");
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
		g_mutex_unlock(&server->rs_mtx);
		fprintf(stderr, "NOT accepting %p\n", conn);
		return (-1);
	}
	server->rs_refcnt++; /* conn has reference */

	g_rw_lock_writer_lock(&server->rs_connections_rwlock);
	server->rs_connections = g_list_append(server->rs_connections, conn);
	conn->rco_conn_ref(conn,true);
	g_rw_lock_writer_unlock(&server->rs_connections_rwlock);

	server->rs_conn_made++;
	g_mutex_unlock(&server->rs_mtx);
	return (0);
}

/* undo accept */
static void
rpc_server_disconnect(rpc_server_t server, rpc_connection_t conn)
{
 
        GList *iter = NULL;
        struct rpc_connection *comp = NULL;

	//debugf("Disconnecting: %p, closed == %d\n", conn, server->rs_closed);
	fprintf(stderr, "Disconnecting: %p, closed == %d\n", conn, server->rs_closed);

	g_mutex_lock(&server->rs_mtx);
	if (server->rs_closed) {
		g_mutex_unlock(&server->rs_mtx);
		return;
	}

	g_assert(conn->rco_aborted);

        g_rw_lock_writer_lock(&server->rs_connections_rwlock);
        for (iter = server->rs_connections; iter != NULL; iter = iter->next) {
                comp = iter->data;
                if (comp == conn) {
                        server->rs_connections =
                            g_list_remove_link(server->rs_connections, iter);
			fprintf(stderr, "server disconnect deref conn %p\n", conn);
			conn->rco_conn_ref(conn,false);
                        break;
                }
        }
 
	g_rw_lock_writer_unlock(&server->rs_connections_rwlock);
	g_mutex_unlock(&server->rs_mtx);

	return;
}

static void *
rpc_server_worker(void *arg)
{
	rpc_server_t server = arg;

	g_main_context_push_thread_default(server->rs_g_context);
	g_main_loop_run(server->rs_g_loop);
	fprintf(stderr, "server thread EXITING\n");
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
	if (transport->listen(server, server->rs_uri, NULL) == 0)
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

	rpc_server_t server;

	if (rpc_server_find(uri, context) != NULL) {
		debugf("duplicate server %s", uri);
		rpc_set_last_errorf(EEXIST, "Server %s already exists", uri);
		return (NULL);
	}
	debugf("creating server %s", uri);

	server = g_malloc0(sizeof(*server));
	server->rs_uri = uri;
	server->rs_paused = true;
	server->rs_calls = g_queue_new();
	server->rs_context = context;
	server->rs_accept = &rpc_server_accept;
	server->rs_valid = &rpc_server_valid;
	server->rs_disconnect = &rpc_server_disconnect;
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
		rpc_connection_send_event(conn, path, interface, name,
		    rpc_retain(args));
	}
	g_rw_lock_reader_unlock(&server->rs_connections_rwlock);
}

int
rpc_server_dispatch(rpc_server_t server, struct rpc_inbound_call *call)
{
	int ret;
	rpc_object_t sdict;

	g_mutex_lock(&server->rs_calls_mtx);

	if (server->rs_closed) {
		g_mutex_unlock(&server->rs_calls_mtx);
		return (-1);
	}

	if (server->rs_paused || !g_queue_is_empty(server->rs_calls))  {
		sdict = rpc_dictionary_create();
		rpc_dictionary_set_string(sdict, "method", call->ric_name);
		if (call->ric_path)
			rpc_dictionary_set_string(sdict, "path",
			    call->ric_path);
		if (call->ric_interface)
			rpc_dictionary_set_string(sdict, "interface",
			    call->ric_interface);
		call->ric_strings = sdict;

		g_queue_push_tail(server->rs_calls, call);
		g_mutex_unlock(&server->rs_calls_mtx);
		return (0);
	}
	ret = server->rs_closed ? -1 :
	    rpc_context_dispatch(server->rs_context, call);
	g_mutex_unlock(&server->rs_calls_mtx);
	return (ret);
}

/*called with rs_call_mtx held*/
static void
server_queue_purge(rpc_server_t server)
{
	struct rpc_inbound_call *icall;

	while (!g_queue_is_empty(server->rs_calls)) {
		icall = g_queue_pop_head(server->rs_calls);
		if (icall->ric_strings != NULL) {
			icall->ric_name = rpc_dictionary_get_string(
			    icall->ric_strings, "method");
			icall->ric_interface = rpc_dictionary_get_string(
			    icall->ric_strings, "interface");
			icall->ric_path = rpc_dictionary_get_string(
			    icall->ric_strings, "path");
		}
		if (server->rs_closed ||
		    (rpc_context_dispatch(server->rs_context, icall) != 0))
			rpc_connection_close_inbound_call(icall);
	}
}


void
rpc_server_resume(rpc_server_t server)
{

	g_mutex_lock(&server->rs_calls_mtx);
	if (server->rs_closed) {
		return;
		g_mutex_unlock(&server->rs_calls_mtx);
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
//	bool m;

//	m = g_mutex_trylock(&server->rs_mtx);
//	g_assert(m);	 
	g_mutex_lock(&server->rs_mtx);
	if (server->rs_closed && server->rs_refcnt == 1) {
        	g_rw_lock_writer_lock(&server->rs_context->rcx_server_rwlock);
        	g_ptr_array_remove(server->rs_context->rcx_servers, server);
        	g_rw_lock_writer_unlock(&server->rs_context->rcx_server_rwlock);

		g_mutex_unlock(&server->rs_mtx);
		fprintf(stderr,"Server closed m: %d r: %d, c: %d, f: %d, a: %d\n",
			server->rs_conn_made, server->rs_conn_refused, 
			server->rs_conn_closed, server->rs_conn_freed, 
			server->rs_conn_aborted);
		g_free(server);
		fprintf(stderr, "FREED SERVER %p\n", server);
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
			fprintf(stderr, "server close deref conn %p\n", conn);
			deref = rpc_connection_close(conn);
			server->rs_conn_aborted++;

			if (deref == -1) /* disconnect must fail, deref here */
				conn->rco_conn_ref(conn, false);
		}
		debugf("DROPPED");
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
