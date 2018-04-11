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
#include <glib.h>
#include <rpc/object.h>
#include <rpc/server.h>
#include "internal.h"

static int rpc_server_accept(rpc_server_t, rpc_connection_t);
static void rpc_server_cleanup(rpc_server_t server);

static void
rpc_server_cleanup(rpc_server_t server)
{
        g_main_context_invoke(server->rs_g_context,
            (GSourceFunc)rpc_kill_main_loop, server->rs_g_loop);
        g_thread_join(server->rs_thread);
        g_main_loop_unref(server->rs_g_loop);
        g_main_context_unref(server->rs_g_context);
}

static int
rpc_server_accept(rpc_server_t server, rpc_connection_t conn)
{

	if (server->rs_closed)
		return (-1);
	server->rs_connections = g_list_append(server->rs_connections, conn);
	return (0);
}

static void *
rpc_server_worker(void *arg)
{
	rpc_server_t server = arg;

	g_main_context_push_thread_default(server->rs_g_context);
	g_main_loop_run(server->rs_g_loop);
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
		debugf("No such transport");
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
rpc_server_create(const char *uri, rpc_context_t context)
{

	rpc_server_t server;

	debugf("creating server");

	server = g_malloc0(sizeof(*server));
	server->rs_uri = uri;
	server->rs_paused = true;
	server->rs_context = context;
	server->rs_accept = &rpc_server_accept;
	server->rs_g_context = g_main_context_new();
	server->rs_g_loop = g_main_loop_new(server->rs_g_context, false);
	server->rs_thread = g_thread_new("librpc server", rpc_server_worker,
	    server);
	g_cond_init(&server->rs_cv);
	g_mutex_init(&server->rs_mtx);

	g_mutex_lock(&server->rs_mtx);
	g_main_context_invoke(server->rs_g_context, rpc_server_listen, server);

	while (server->rs_error == NULL && !server->rs_operational)
		g_cond_wait(&server->rs_cv, &server->rs_mtx);

	if (server->rs_error != NULL) {
                debugf("failed server");
		rpc_set_last_rpc_error(server->rs_error);
                rpc_server_cleanup(server);
                return(NULL);
	}

	g_ptr_array_add(context->rcx_servers, server);
	g_mutex_unlock(&server->rs_mtx);
	return (server);
}

void
rpc_server_broadcast_event(rpc_server_t server, const char *path,
    const char *interface, const char *name, rpc_object_t args)
{
	GList *item;

        if (server->rs_closed)
		return;

	for (item = g_list_first(server->rs_connections); item;
	     item = item->next) {
		rpc_connection_t conn = item->data;
		rpc_connection_send_event(conn, path, interface, name,
		    rpc_retain(args));
	}
}

int
rpc_server_dispatch(rpc_server_t server, struct rpc_inbound_call *call)
{
	int ret;

	g_mutex_lock(&server->rs_mtx);
	while (server->rs_paused)
		g_cond_wait(&server->rs_cv, &server->rs_mtx);

	ret = rpc_context_dispatch(server->rs_context, call);
	g_mutex_unlock(&server->rs_mtx);
	return (ret);
}

void
rpc_server_resume(rpc_server_t server)
{
	g_mutex_lock(&server->rs_mtx);
	server->rs_paused = false;
	g_cond_broadcast(&server->rs_cv);
	g_mutex_unlock(&server->rs_mtx);
}

int
rpc_server_close(rpc_server_t server)
{
	struct rpc_connection *conn;
	GList *iter = NULL;
	int ret = 0;

	if (server->rs_teardown)
		ret = server->rs_teardown(server);
        else {
		rpc_set_last_errorf(ENOTSUP, "Not supported by transport");	
		return (-1);
	}

	/* Drop all connections */
	for (iter = server->rs_connections; iter != NULL; iter = iter->next) {
		conn = iter->data;
		conn->rco_abort(conn->rco_arg);
	}
	/* stop listener thread. */
	rpc_server_cleanup(server);
        g_free(server);

	return (ret);
}
