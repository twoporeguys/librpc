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

static bool rpc_context_path_is_valid(const char *path);
static rpc_object_t rpc_get_objects(void *cookie, rpc_object_t args);
static rpc_object_t rpc_get_interfaces(void *cookie, rpc_object_t args);
static rpc_object_t rpc_get_methods(void *cookie, rpc_object_t args);
static rpc_object_t rpc_interface_exists(void *cookie, rpc_object_t args);

static void
rpc_context_tp_handler(gpointer data, gpointer user_data)
{
	struct rpc_context *context = user_data;
	struct rpc_inbound_call *call = data;
	struct rpc_method *method = call->ric_method;
	rpc_connection_t conn = call->ric_conn;
	rpc_object_t result;

	if (method == NULL) {
		rpc_function_error(call, ENOENT, "Method not found");
		return;
	}

	call->ric_arg = method->rm_arg;
	call->ric_context = context;
	call->ric_consumer_seqno = 1;

	debugf("method=%p", method);

	if (context->rcx_pre_call_hook != NULL) {

	}

	result = method->rm_block((void *)call, call->ric_args);

	if (result == RPC_FUNCTION_STILL_RUNNING)
		return;

	if (context->rcx_post_call_hook != NULL) {

	}

	if (!call->ric_streaming && !call->ric_responded) {
		rpc_connection_send_response(conn, call->ric_id, result);
		rpc_release(result);
	}

	if (call->ric_streaming && !call->ric_ended)
		rpc_function_end(data);
}

rpc_context_t
rpc_context_create(void)
{
	GError *err;
	rpc_context_t result;

	result = g_malloc0(sizeof(*result));
	result->rcx_root = rpc_instance_new("/", "Root object", NULL);
	result->rcx_instances = g_hash_table_new(g_str_hash, g_str_equal);
	result->rcx_threadpool = g_thread_pool_new(rpc_context_tp_handler,
	    result, g_get_num_processors() * 4, true, &err);

	rpc_instance_register_func(result->rcx_root, "librpc.Discoverable",
	    "get_instances", NULL, rpc_get_objects);

	rpc_context_register_instance(result, result->rcx_root);
	return (result);
}

void
rpc_context_free(rpc_context_t context)
{

	g_thread_pool_free(context->rcx_threadpool, true, true);
	rpc_instance_free(context->rcx_root);
	g_free(context);
}

int
rpc_context_dispatch(rpc_context_t context, struct rpc_inbound_call *call)
{
	GError *err = NULL;
	rpc_instance_t instance = NULL;

	debugf("call=%p, name=%s", call, call->ric_name);

	if (call->ric_path == NULL)
		instance = context->rcx_root;

	if (instance == NULL)
		instance = rpc_context_find_instance(context, call->ric_path);

	if (instance == NULL) {
		rpc_function_error(call, ENOENT, "Instance not found");
		return (-1);
	}

	call->ric_instance = instance;
	call->ric_method = rpc_instance_find_method(instance,
	    call->ric_interface, call->ric_name);

	if (call->ric_method == NULL) {
		rpc_function_error(call, ENOENT, "Method not found");
		return (-1);
	}

	g_thread_pool_push(context->rcx_threadpool, call, &err);
	if (err != NULL) {
		rpc_function_error(call, EFAULT, "Cannot submit call");
		return (-1);
	}

	return (0);
}

rpc_instance_t
rpc_context_find_instance(rpc_context_t context, const char *path)
{

	return (g_hash_table_lookup(context->rcx_instances, path));
}

struct rpc_method *
rpc_context_find_method(rpc_context_t context, const char *interface,
    const char *name)
{

	return (rpc_instance_find_method(context->rcx_root, interface,
	    name));
}

void
rpc_instance_emit_event(rpc_instance_t instance, const char *interface,
    const char *name, rpc_object_t args)
{
	g_mutex_lock(&instance->ri_mtx);

	if (instance->ri_context && instance->ri_context->rcx_server) {
		rpc_server_broadcast_event(instance->ri_context->rcx_server,
		    instance->ri_path, interface, name, args);
	}

	g_mutex_unlock(&instance->ri_mtx);
}

int
rpc_instance_register_property(rpc_instance_t instance, const char *name,
    rpc_object_t value, int rights)
{
	struct rpc_instance_property *prop;

	prop = g_malloc0(sizeof(*prop));
	g_mutex_init(&prop->rip_mtx);
	prop->rip_name = g_strdup(name);
	prop->rip_rights = rights;
	prop->rip_value = rpc_retain(value);
	prop->rip_watchers = g_ptr_array_new();

	g_hash_table_insert(instance->ri_properties, prop->rip_name, prop);
	rpc_instance_emit_event(instance, "librpc.Observable",
	    "property_added", rpc_object_pack("{s,v,b,b}",
	        "name", name,
	        "value", value,
	        "readable", rights & RPC_PROPERTY_READ,
	        "writable", rights & RPC_PROPERTY_WRITE));

	return (0);
}

void
rpc_instance_unregister_property(rpc_instance_t instance, const char *name)
{

	g_hash_table_remove(instance->ri_properties, name);
	rpc_instance_emit_event(instance, "librpc.Observable",
	    "property_removed", rpc_object_pack("{s}", "name", name));
}

rpc_object_t
rpc_instance_get_property(rpc_instance_t instance, const char *name)
{
	struct rpc_instance_property *prop;

	prop = g_hash_table_lookup(instance->ri_properties, name);
	if (prop == NULL) {
		rpc_set_last_error(ENOENT, "Property not found", NULL);
		return (NULL);
	}

	return (prop->rip_value);
}

void
rpc_instance_set_property(rpc_instance_t instance, const char *name,
    rpc_object_t value)
{
	struct rpc_instance_property *prop;
	rpc_property_watcher_t cb;
	int i;

	prop = g_hash_table_lookup(instance->ri_properties, name);
	if (prop == NULL)
		return;

	g_mutex_lock(&prop->rip_mtx);
	rpc_release(prop->rip_value);
	prop->rip_value = rpc_retain(prop->rip_value);

	for (i = 0; i < prop->rip_watchers->len; i++) {
		cb = g_ptr_array_index(prop->rip_watchers, i);
		cb(name, value);
	}

	rpc_instance_emit_event(instance, "librpc.Observable",
	    "property_changed", rpc_object_pack("{s,v}",
	        "name", name,
	        "value", value));

	g_mutex_unlock(&prop->rip_mtx);
}

void *
rpc_instance_watch_property(rpc_instance_t instance, const char *name,
    rpc_property_watcher_t fn)
{
	struct rpc_instance_property *prop;
	rpc_property_watcher_t realcb;

	prop = g_hash_table_lookup(instance->ri_properties, name);
	if (prop == NULL) {
		rpc_set_last_error(ENOENT, "Property not found", NULL);
		return (NULL);
	}

	g_mutex_lock(&prop->rip_mtx);
	realcb = Block_copy(fn);
	g_ptr_array_add(prop->rip_watchers, realcb);
	g_mutex_unlock(&prop->rip_mtx);

	return (realcb);
}

void
rpc_instance_unwatch_property(rpc_instance_t instance, const char *name,
    void *cookie)
{
	struct rpc_instance_property *prop;
	rpc_property_watcher_t realcb;

	prop = g_hash_table_lookup(instance->ri_properties, name);
	if (prop == NULL)
		return;

	g_mutex_lock(&prop->rip_mtx);
	g_ptr_array_remove(prop->rip_watchers, cookie);
	g_mutex_unlock(&prop->rip_mtx);
}


int
rpc_context_register_instance(rpc_context_t context, rpc_instance_t instance)
{

	g_mutex_lock(&context->rcx_mtx);

	if (g_hash_table_contains(context->rcx_instances, instance->ri_path)) {
		rpc_set_last_error(EEXIST, "Instance already exists", NULL);
		g_mutex_unlock(&context->rcx_mtx);
		return (-1);
	}

	if (context->rcx_server != NULL) {
		rpc_server_broadcast_event(context->rcx_server, "/",
		    "librpc.Discoverable", "object_added",
		    rpc_string_create(instance->ri_path));
	}

	g_hash_table_insert(context->rcx_instances, instance->ri_path, instance);
	g_mutex_unlock(&context->rcx_mtx);
	return (0);
}

void
rpc_context_unregister_instance(rpc_context_t context, const char *path)
{

	g_mutex_lock(&context->rcx_mtx);

	if (g_hash_table_remove(context->rcx_instances, path)) {
		if (context->rcx_server != NULL) {
			rpc_server_broadcast_event(context->rcx_server, "/",
			    "librpc.Discoverable", "object_removed",
			    rpc_string_create(path));
		}
	}

	g_mutex_unlock(&context->rcx_mtx);
}

int
rpc_context_register_method(rpc_context_t context, struct rpc_method *m)
{

	return (rpc_instance_register_method(context->rcx_root, m));
}

int
rpc_context_register_block(rpc_context_t context, const char *interface,
    const char *name, void *arg, rpc_function_t func)
{

	return (rpc_instance_register_block(context->rcx_root, interface,
	    name, arg, func));
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
rpc_context_unregister_method(rpc_context_t context, const char *interface,
    const char *name)
{

	return (rpc_instance_unregister_method(context->rcx_root, interface,
	    name));
}

inline void *
rpc_function_get_arg(void *cookie)
{
	struct rpc_inbound_call *call = cookie;

	return (call->ric_arg);
}

inline rpc_context_t
rpc_function_get_context(void *cookie)
{
	struct rpc_inbound_call *call = cookie;

	return (call->ric_context);
}

inline rpc_instance_t
rpc_function_get_instance(void *cookie)
{
	struct rpc_inbound_call *call = cookie;

	return (call->ric_instance);
}

const char *
rpc_function_get_name(void *cookie)
{
	struct rpc_inbound_call *call = cookie;

	return (call->ric_name);
}

const char *
rpc_function_get_path(void *cookie)
{
	struct rpc_inbound_call *call = cookie;

	return (call->ric_path);
}

const char *
rpc_function_get_interface(void *cookie)
{
	struct rpc_inbound_call *call = cookie;

	return (call->ric_interface);
}

void
rpc_function_respond(void *cookie, rpc_object_t object)
{
	struct rpc_inbound_call *call = cookie;

	rpc_connection_send_response(call->ric_conn, call->ric_id, object);
	rpc_connection_close_inbound_call(call);
	rpc_release(object);
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
	call->ric_responded = true;
	g_free(msg);
}

void
rpc_function_error_ex(void *cookie, rpc_object_t exception)
{
	struct rpc_inbound_call *call = cookie;

	rpc_connection_send_errx(call->ric_conn, call->ric_id, exception);
	call->ric_responded = true;
}

int
rpc_function_yield(void *cookie, rpc_object_t fragment)
{
	struct rpc_inbound_call *call = cookie;
	struct rpc_context *context = call->ric_context;

	g_mutex_lock(&call->ric_mtx);

	while (call->ric_producer_seqno == call->ric_consumer_seqno && !call->ric_aborted)
		g_cond_wait(&call->ric_cv, &call->ric_mtx);

	if (call->ric_aborted) {
		g_mutex_unlock(&call->ric_mtx);
		rpc_release(fragment);
		return (-1);
	}

	if (context->rcx_pre_call_hook != NULL) {

	}

	rpc_connection_send_fragment(call->ric_conn, call->ric_id,
	    call->ric_producer_seqno, fragment);

	call->ric_producer_seqno++;
	call->ric_streaming = true;
	g_mutex_unlock(&call->ric_mtx);
	rpc_release(fragment);
	return (0);
}

void
rpc_function_end(void *cookie)
{
	struct rpc_inbound_call *call = cookie;

	g_mutex_lock(&call->ric_mtx);

	while (call->ric_producer_seqno == call->ric_consumer_seqno && !call->ric_aborted)
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

bool
rpc_function_should_abort(void *cookie)
{
	struct rpc_inbound_call *call = cookie;

	return (call->ric_aborted);
}

rpc_instance_t
rpc_instance_new(const char *path, const char *descr, void *arg)
{
	rpc_instance_t result;

	if (!rpc_context_path_is_valid(path)) {
		rpc_set_last_error(EINVAL, "Invalid path", NULL);
		return (NULL);
	}

	result = g_malloc0(sizeof(*result));
	g_mutex_init(&result->ri_mtx);
	result->ri_path = g_strdup(path);
	result->ri_descr = g_strdup(descr);
	result->ri_subscriptions = g_hash_table_new(g_str_hash, g_str_equal);
	result->ri_interfaces = g_hash_table_new(g_str_hash, g_str_equal);
	result->ri_arg = arg;

	rpc_instance_register_func(result, "librpc.Introspectable",
	    "get_interfaces", NULL, rpc_get_interfaces);
	rpc_instance_register_func(result, "librpc.Introspectable",
	    "get_methods", NULL, rpc_get_methods);
	rpc_instance_register_func(result, "librpc.Introspectable",
	    "interface_exists", NULL, rpc_interface_exists);

	return (result);
}

void *rpc_instance_get_arg(rpc_instance_t instance)
{

	g_assert_nonnull(instance);

	return (instance->ri_arg);
}

const char *rpc_instance_get_path(rpc_instance_t instance)
{

	g_assert_nonnull(instance);

	return (instance->ri_path);
}

void rpc_instance_free(rpc_instance_t instance)
{
	g_assert_nonnull(instance);

	g_free(instance->ri_path);
	g_hash_table_destroy(instance->ri_interfaces);
	g_free(instance);
}

struct rpc_method *
rpc_instance_find_method(rpc_instance_t instance, const char *interface,
    const char *name)
{
	GHashTable *methods;

	methods = g_hash_table_lookup(instance->ri_interfaces,
	    interface != NULL ? interface : "");
	if (methods == NULL)
		return (NULL);

	return (g_hash_table_lookup(methods, name));
}

int
rpc_instance_register_method(rpc_instance_t instance, struct rpc_method *m)
{
	struct rpc_method *method;
	const char *interface;
	GHashTable *methods;

	interface = m->rm_interface != NULL ? m->rm_interface : "";
	method = g_memdup(m, sizeof(*m));
	methods = g_hash_table_lookup(instance->ri_interfaces, interface);

	if (methods == NULL) {
		methods = g_hash_table_new(g_str_hash, g_str_equal);
		g_hash_table_insert(instance->ri_interfaces,
		    (gpointer)interface, methods);
	}

	g_hash_table_insert(methods, (gpointer)m->rm_name, method);
	return (0);
}

int
rpc_instance_register_block(rpc_instance_t instance, const char *interface,
    const char *name, void *arg, rpc_function_t func)
{
	struct rpc_method method;

	method.rm_name = g_strdup(name);
	method.rm_interface = g_strdup(interface);
	method.rm_block = Block_copy(func);
	method.rm_arg = arg;

	return (rpc_instance_register_method(instance, &method));
}

int
rpc_instance_register_func(rpc_instance_t instance, const char *interface,
    const char *name, void *arg, rpc_function_f func)
{
	rpc_function_t fn = ^(void *cookie, rpc_object_t args) {
		return (func(cookie, args));
	};

	return (rpc_instance_register_block(instance, interface, name, arg, fn));
}

int
rpc_instance_unregister_method(rpc_instance_t instance, const char *interface,
    const char *name)
{
	GHashTable *methods;
	struct rpc_method *method;

	methods = g_hash_table_lookup(instance->ri_interfaces, interface);
	if (methods == NULL) {
		errno = ENOENT;
		return (-1);
	}

	method = g_hash_table_lookup(methods, name);
	if (method == NULL) {
		errno = ENOENT;
		return (-1);
	}

	Block_release(method->rm_block);
	g_hash_table_remove(methods, name);
	return (0);
}

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
		fragment = rpc_object_pack("{s,s}",
		    "path", v->ri_path,
		    "description", v->ri_descr);

		if (rpc_function_yield(cookie, fragment) != 0)
			goto done;
	}

done:
	return ((rpc_object_t)NULL);
}

static rpc_object_t
rpc_get_interfaces(void *cookie, rpc_object_t args __unused)
{
	GHashTableIter iter;
	const char *k;
	void *v;
	rpc_object_t fragment;
	rpc_instance_t instance = rpc_function_get_instance(cookie);

	g_hash_table_iter_init(&iter, instance->ri_interfaces);

	while (g_hash_table_iter_next(&iter, (gpointer)&k, (gpointer)&v)) {
		fragment = rpc_string_create(k);
		if (rpc_function_yield(cookie, fragment) != 0)
			goto done;
	}

done:
	return ((rpc_object_t)NULL);
}

static rpc_object_t
rpc_get_methods(void *cookie, rpc_object_t args)
{
	GHashTable *methods;
	GHashTableIter iter;
	const char *interface;
	const char *k;
	void *v;
	rpc_object_t fragment;
	rpc_instance_t instance = rpc_function_get_instance(cookie);

	if (rpc_object_unpack(args, "[s]", &interface) < 1) {
		rpc_function_error(cookie, EINVAL, "Invalid arguments passed");
		return (NULL);
	}

	methods = g_hash_table_lookup(instance->ri_interfaces, interface);
	if (methods == NULL) {
		rpc_function_error(cookie, ENOENT, "Interface not found");
		return (NULL);
	}

	g_hash_table_iter_init(&iter, methods);
	while (g_hash_table_iter_next(&iter, (gpointer)&k, (gpointer)&v)) {
		fragment = rpc_string_create(k);
		if (rpc_function_yield(cookie, fragment) != 0)
			goto done;
	}

done:
	return ((rpc_object_t)NULL);
}

static rpc_object_t
rpc_interface_exists(void *cookie, rpc_object_t args)
{
	rpc_instance_t instance = rpc_function_get_instance(cookie);
	const char *interface;

	if (rpc_object_unpack(args, "[s]", &interface) < 1) {
		rpc_function_error(cookie, EINVAL, "Invalid arguments passed");
		return (NULL);
	}

	return (rpc_bool_create((bool)g_hash_table_contains(
	    instance->ri_interfaces, interface)));
}

static bool
rpc_context_path_is_valid(const char *path)
{
	size_t length;

	length = strlen(path);

	if (length < 1)
		return (false);

	if (path[0] != '/')
		return (false);

	if (length > 1 && path[length - 1] == '/')
		return (false);

	return (true);
}
