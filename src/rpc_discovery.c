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

#include <rpc/service.h>
#include <rpc/discovery.h>
#include <glib.h>
#include "internal.h"

static rpc_object_t
rpc_get_objects(void *cookie, rpc_object_t args __unused)
{
	rpc_context_t context = rpc_function_get_context(cookie);
	GHashTableIter iter;
	const char *k;
	rpc_instance_t v;
	rpc_object_t fragment;

	g_hash_table_iter_init(&iter, context->rcx_instances);

	while (g_hash_table_iter_next(&iter, (gpointer)&k, (gpointer)&v)) {
		fragment = rpc_object_pack("{s}",
		    "path", v->ri_path);

		if (rpc_function_yield(cookie, fragment) != 0)
			goto done;
	}

done:
	return ((rpc_object_t)NULL);
}

static rpc_object_t
rpc_get_methods(void *cookie, rpc_object_t args __unused)
{
	GHashTableIter iter;
	const char *k;
	struct rpc_method *m;
	rpc_object_t fragment;
	rpc_context_t context = rpc_function_get_arg(cookie);

	g_hash_table_iter_init(&iter, context->rcx_methods);

	while (g_hash_table_iter_next(&iter, (gpointer)&k, (gpointer)&m)) {
		fragment = rpc_dictionary_create();
		rpc_dictionary_set_string(fragment, "name", m->rm_name);
		rpc_dictionary_set_string(fragment, "description", m->rm_description);
		if (rpc_function_yield(cookie, fragment) != 0)
			goto done;
	}

done:
	return ((rpc_object_t)NULL);
}

int
rpc_discovery_register(rpc_context_t context)
{

	return (rpc_context_register_func(context, "discovery.get_methods",
	    "Returns a list of all registered methods", context,
	    &rpc_get_methods));
}

int
rpc_discovery_destroy(rpc_context_t context)
{

	return (rpc_context_unregister_method(context, "discovery.get_methods"));
}

