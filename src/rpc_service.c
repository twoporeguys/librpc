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

#include <Block.h>
#include <stdlib.h>
#include <rpc/object.h>
#include <rpc/connection.h>
#include <glib.h>
#include "internal.h"

struct rpc_method
{
	const char *	rm_name;
    	const char *	rm_description;
    	rpc_function_f  rm_handler;
    	rpc_function_t  rm_block;
    	void *		rm_arg;
};

static void
rpc_context_tp_handler(gpointer data, gpointer user_data)
{
	struct rpc_inbound_call *call = data;
	struct rpc_context *context = user_data;
	struct rpc_method *method;

	if (method->rm_handler) {}
}

rpc_context_t
rpc_context_create(void)
{
	GError *err;
	rpc_context_t result;

	result = g_malloc0(sizeof(*result));
	result->rcx_methods = g_hash_table_new(g_str_hash, g_str_equal);
	result->rcx_threadpool = g_thread_pool_new(rpc_context_tp_handler,
	    result, g_get_num_processors(), true, &err);

	return (result);
}

void
rpc_context_free(rpc_context_t context)
{

}

int
rpc_context_dispatch(rpc_context_t context, void *cookie, const char *name,
    rpc_object_t args)
{
	struct rpc_method *method;

	method = g_hash_table_lookup(context->rcx_methods, (gconstpointer)name);
	if (method == NULL) {

	}
}

int
rpc_context_register_method(rpc_context_t context, const char *name,
    const char *descr, rpc_function_t func, void *arg, int flags)
{
	struct rpc_method *method;

	method = g_malloc0(sizeof(*method));
	method->rm_name = g_strdup(name);
	method->rm_description = g_strdup(descr);
	method->rm_block = Block_copy(func);
	method->rm_arg = arg;
	g_hash_table_insert(context->rcx_methods, (gpointer)method->rm_name,
	    method);

	return (0);
}

int
rpc_context_register_method_f(rpc_context_t context, const char *name,
    const char *descr, rpc_function_f func, void *arg, int flags)
{
	struct rpc_method *method;

	method = g_malloc0(sizeof(*method));
	method->rm_name = g_strdup(name);
	method->rm_description = g_strdup(descr);
	method->rm_handler = func;
	method->rm_arg = arg;
	g_hash_table_insert(context->rcx_methods, (gpointer)method->rm_name,
	    method);

	return (0);
}

void
rpc_function_respond(void *cookie, rpc_object_t object)
{
	struct rpc_inbound_call *call = cookie;

}

void
rpc_function_error(void *cookie, int code, const char *message, ...)
{
	struct rpc_inbound_call *call = cookie;

}

void
rpc_function_error_ex(void *cookie, rpc_object_t exception)
{
	struct rpc_inbound_call *call = cookie;

}

void
rpc_function_produce(void *cookie, rpc_object_t fragment)
{
	struct rpc_inbound_call *call = cookie;

}

void
rpc_function_end(void *cookie)
{
	struct rpc_inbound_call *call = cookie;

}
