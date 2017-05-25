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

static int
rpc_server_accept(rpc_server_t server, rpc_connection_t conn)
{

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

	g_mutex_lock(&server->rs_mtx);

	transport = rpc_find_transport(g_uri_parse_scheme(server->rs_uri));
	if (transport == NULL) {
		errno = ENXIO;
		goto done;
	}

	debugf("selected transport %s", transport->name);
	server->rs_flags = transport->flags;
	transport->listen(server, server->rs_uri, NULL);
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
	server->rs_context = context;
	server->rs_accept = &rpc_server_accept;
	server->rs_subscriptions = g_hash_table_new(g_str_hash, g_str_equal);
	server->rs_g_context = g_main_context_new();
	server->rs_g_loop = g_main_loop_new(server->rs_g_context, false);
	server->rs_thread = g_thread_new("librpc server", rpc_server_worker,
	    server);
	g_cond_init(&server->rs_cv);
	g_mutex_init(&server->rs_mtx);
	g_mutex_init(&server->rs_subscription_mtx);

	g_mutex_lock(&server->rs_mtx);
	g_main_context_invoke(server->rs_g_context, rpc_server_listen, server);

	while (!server->rs_operational)
		g_cond_wait(&server->rs_cv, &server->rs_mtx);

	g_mutex_unlock(&server->rs_mtx);
	return (server);
}

void
rpc_server_broadcast_event(rpc_server_t server, const char *name,
    rpc_object_t args)
{
	GList *item;

	for (item = g_list_first(server->rs_connections); item;
	     item = item->next) {
		rpc_connection_t conn = item->data;
		rpc_connection_send_event(conn, name, args);
	}
}

int
rpc_server_dispatch(rpc_server_t server, struct rpc_inbound_call *call)
{

	return (rpc_context_dispatch(server->rs_context, call));
}

int
rpc_server_start(rpc_server_t server, bool background)
{


}

int
rpc_server_stop(rpc_server_t server)
{

}

int
rpc_server_close(rpc_server_t server)
{

}
