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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <rpc/object.h>
#include <rpc/connection.h>
#include <rpc/service.h>
#include <glib.h>
#include <glib/gprintf.h>
#include "internal.h"

static void
rpc_context_tp_handler(gpointer data, gpointer user_data __unused)
{
	struct rpc_inbound_call *call = data;
	struct rpc_method *method = call->ric_method;
	rpc_connection_t conn = call->ric_conn;
	rpc_object_t result;

	if (method == NULL) {
		rpc_function_error(call, ENOENT, "Method not found");
		return;
	}

	call->ric_arg = method->rm_arg;
	call->ric_consumer_seqno = 1;

	debugf("method=%p", method);
	result = method->rm_block((void *)call, call->ric_args);

	if (result == RPC_FUNCTION_STILL_RUNNING)
		return;

	if (!call->ric_streaming && !call->ric_responded)
		rpc_connection_send_response(conn, call->ric_id, result);

	if (call->ric_streaming && !call->ric_ended) {
		rpc_connection_send_end(conn, call->ric_id,
		    call->ric_producer_seqno);
	}
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

	g_thread_pool_free(context->rcx_threadpool, true, true);
	g_hash_table_destroy(context->rcx_methods);
	g_free(context);
}

int
rpc_context_dispatch(rpc_context_t context, struct rpc_inbound_call *call)
{
	GError *err = NULL;

	debugf("call=%p, name=%s", call, call->ric_name);

	call->ric_method = g_hash_table_lookup(context->rcx_methods,
	    call->ric_name);

	g_thread_pool_push(context->rcx_threadpool, call, &err);
	if (err != NULL) {

	}

	return (0);
}

struct rpc_method *
rpc_context_find_method(rpc_context_t context, const char *name)
{

	return (g_hash_table_lookup(context->rcx_methods, name));
}

int
rpc_context_register_method(rpc_context_t context, struct rpc_method *m)
{
	struct rpc_method *method;

	method = g_memdup(m, sizeof(*m));
	g_hash_table_insert(context->rcx_methods, (gpointer)m->rm_name, method);
	return (0);
}

int
rpc_context_register_block(rpc_context_t context, const char *name,
    const char *descr, void *arg, rpc_function_t func)
{
	struct rpc_method method;

	method.rm_name = g_strdup(name);
	method.rm_description = g_strdup(descr);
	method.rm_block = Block_copy(func);
	method.rm_arg = arg;

	return (rpc_context_register_method(context, &method));
}

int
rpc_context_register_func(rpc_context_t context, const char *name,
    const char *descr, void *arg, rpc_function_f func)
{
	rpc_function_t fn = ^(void *cookie, rpc_object_t args) {
		return (func(cookie, args));
	};

	return (rpc_context_register_block(context, name, descr, arg, fn));
}

int
rpc_context_unregister_method(rpc_context_t context, const char *name)
{
	struct rpc_method *method;

	method = g_hash_table_lookup(context->rcx_methods, name);
	if (method == NULL) {
		errno = ENOENT;
		return (-1);
	}

	Block_release(method->rm_block);
	g_hash_table_remove(context->rcx_methods, method);
	return (0);
}

inline void *
rpc_function_get_arg(void *cookie)
{
	struct rpc_inbound_call *call = cookie;

	return (call->ric_arg);
}

void
rpc_function_respond(void *cookie, rpc_object_t object)
{
	struct rpc_inbound_call *call = cookie;

	rpc_connection_send_response(call->ric_conn, call->ric_id, object);
	rpc_connection_close_inbound_call(call);
}

void
rpc_function_error(void *cookie, int code, const char *message, ...)
{
	struct rpc_inbound_call *call = cookie;
	char *msg;
	va_list ap;

	va_start(ap, message);
	g_vasprintf(&msg, message, ap);
	va_end(ap);
	rpc_connection_send_err(call->ric_conn, call->ric_id, code, msg);
	g_free(msg);
}

void
rpc_function_error_ex(void *cookie, rpc_object_t exception)
{
	struct rpc_inbound_call *call = cookie;

	rpc_connection_send_errx(call->ric_conn, call->ric_id, exception);
}

int
rpc_function_yield(void *cookie, rpc_object_t fragment)
{
	struct rpc_inbound_call *call = cookie;

	g_mutex_lock(&call->ric_mtx);

	while (call->ric_producer_seqno == call->ric_consumer_seqno || call->ric_aborted)
		g_cond_wait(&call->ric_cv, &call->ric_mtx);

	if (call->ric_aborted) {
		g_mutex_unlock(&call->ric_mtx);
		return (-1);
	}

	rpc_connection_send_fragment(call->ric_conn, call->ric_id,
	    call->ric_producer_seqno, fragment);

	call->ric_producer_seqno++;
	call->ric_streaming = true;
	g_mutex_unlock(&call->ric_mtx);

	return (0);
}

void
rpc_function_end(void *cookie)
{
	struct rpc_inbound_call *call = cookie;

	g_mutex_lock(&call->ric_mtx);

	while (call->ric_producer_seqno < call->ric_consumer_seqno || call->ric_aborted)
		g_cond_wait(&call->ric_cv, &call->ric_mtx);

	if (call->ric_aborted) {
		g_mutex_unlock(&call->ric_mtx);
		return;
	}

	rpc_connection_send_end(call->ric_conn, call->ric_id, call->ric_producer_seqno);
	call->ric_producer_seqno++;
	call->ric_streaming = true;
	call->ric_ended = true;
	g_mutex_unlock(&call->ric_mtx);
}
