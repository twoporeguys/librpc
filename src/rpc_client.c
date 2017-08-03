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

#include <rpc/client.h>
#include <glib.h>
#include <gio/gio.h>
#include "internal.h"

static void *
rpc_client_worker(void *arg)
{
	rpc_client_t client = arg;

	g_main_loop_run(client->rci_g_loop);
	return (NULL);
}

rpc_client_t
rpc_client_create(const char *uri, rpc_object_t params)
{
	rpc_client_t client;

	client = g_malloc0(sizeof(*client));
	client->rci_g_context = g_main_context_new();
	client->rci_g_loop = g_main_loop_new(client->rci_g_context, false);
	client->rci_thread = g_thread_new("librpc client", rpc_client_worker,
	    client);
	client->rci_uri = uri;
	client->rci_params = params;

	if (params)
		rpc_retain(params);
	
	g_main_context_push_thread_default(client->rci_g_context);
	client->rci_connection = rpc_connection_create(client->rci_uri, params);
	g_main_context_pop_thread_default(client->rci_g_context);

	if (client->rci_connection == NULL) {
		g_free(client);
		return (NULL);
	}

	client->rci_connection->rco_client = client;
	return (client);
}

rpc_connection_t
rpc_client_get_connection(rpc_client_t client)
{

	return (client->rci_connection);
}

void
rpc_client_close(rpc_client_t client)
{

	rpc_connection_close(client->rci_connection);
	g_main_loop_quit(client->rci_g_loop);
	g_main_loop_unref(client->rci_g_loop);
	g_main_context_unref(client->rci_g_context);
	g_thread_join(client->rci_thread);
	g_free(client);
}
