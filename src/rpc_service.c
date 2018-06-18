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

static bool rpc_context_path_is_valid(const char *);
static rpc_object_t rpc_get_objects(void *, rpc_object_t);
static rpc_object_t rpc_get_interfaces(void *, rpc_object_t);
static rpc_object_t rpc_get_methods(void *, rpc_object_t);
static rpc_object_t rpc_get_events(void *, rpc_object_t);
static rpc_object_t rpc_interface_exists(void *, rpc_object_t);
static rpc_object_t rpc_observable_property_get(void *, rpc_object_t);
static rpc_object_t rpc_observable_property_get_all(void *, rpc_object_t);
static rpc_object_t rpc_observable_property_set(void *, rpc_object_t);

static const struct rpc_if_member rpc_discoverable_vtable[] = {
	RPC_EVENT(instance_added),
	RPC_EVENT(instance_removed),
	RPC_METHOD(get_instances, rpc_get_objects),
	RPC_MEMBER_END
};

static const struct rpc_if_member rpc_introspectable_vtable[] = {
	RPC_EVENT(interface_added),
	RPC_EVENT(interface_removed),
	RPC_METHOD(get_interfaces, rpc_get_interfaces),
	RPC_METHOD(get_methods, rpc_get_methods),
	RPC_METHOD(get_events, rpc_get_events),
	RPC_METHOD(interface_exists, rpc_interface_exists),
	RPC_MEMBER_END
};

static const struct rpc_if_member rpc_observable_vtable[] = {
	RPC_EVENT(changed),
	RPC_METHOD(get, rpc_observable_property_get),
	RPC_METHOD(get_all, rpc_observable_property_get_all),
	RPC_METHOD(set, rpc_observable_property_set),
	RPC_MEMBER_END
};

static void
rpc_context_tp_handler(gpointer data, gpointer user_data)
{
	struct rpc_context *context = user_data;
	struct rpc_call *call = data;
	struct rpc_if_method *method = call->rc_if_method;
	rpc_object_t result;

	if (call->rc_conn->rco_closed) {
		debugf("Can't dispatch call, conn %p closed", call->rc_conn);
		rpc_connection_close_inbound_call(call);
		return;
	}

	if (method == NULL) {
		rpc_function_error(call, ENOENT, "Method not found");
		rpc_connection_close_inbound_call(call);
		return;
	}
	g_assert(call->rc_type == RPC_INBOUND_CALL);

	call->rc_m_arg = method->rm_arg;
	call->rc_context = context;
	call->rc_consumer_seqno = 1;

	debugf("method=%p", method);

	if (context->rcx_pre_call_hook != NULL) {
		context->rcx_pre_call_hook(call, call->rc_args);
		if (call->rc_responded)
			return;
	}

	result = method->rm_block((void *)call, call->rc_args);

	if (result == RPC_FUNCTION_STILL_RUNNING)
		return;

	if (context->rcx_post_call_hook != NULL) {
		context->rcx_post_call_hook(call, result);
		if (call->rc_responded)
			return;
	}

	if (!call->rc_streaming)
		rpc_function_respond(call, result);
	else if (!call->rc_ended)
		rpc_function_end(data);
}

rpc_context_t
rpc_context_create(void)
{
	GError *err;
	rpc_context_t result;

	rpct_init();

	result = g_malloc0(sizeof(*result));
	result->rcx_root = rpc_instance_new(NULL, "/");
	result->rcx_servers = g_ptr_array_new();
	result->rcx_instances = g_hash_table_new(g_str_hash, g_str_equal);
	result->rcx_threadpool = g_thread_pool_new(rpc_context_tp_handler,
	    result, g_get_num_processors() * 4, true, &err);

	rpc_instance_set_description(result->rcx_root, "Root object");
	rpc_context_register_instance(result, result->rcx_root);
	return (result);
}

void
rpc_context_free(rpc_context_t context)
{

	if (context == NULL)
		return;
	g_thread_pool_free(context->rcx_threadpool, true, true);
	rpc_instance_free(context->rcx_root);
	g_free(context);
}

int
rpc_context_dispatch(rpc_context_t context, struct rpc_call *call)
{
	struct rpc_if_member *member;
	GError *err = NULL;
	rpc_instance_t instance = NULL;

	debugf("call=%p, name=%s", call, call->rc_method_name);
	if (call->rc_conn->rco_closed) {
		debugf("Can't dispatch call, conn %p closed", call->rc_conn);
		return (-1);
	}
	if (call->rc_path == NULL)
		instance = context->rcx_root;

	if (instance == NULL)
		instance = rpc_context_find_instance(context, call->rc_path);

	if (instance == NULL) {
		call->rc_err = rpc_error_create(ENOENT, "Instance not found",
		    NULL);
		return (-1);
	}

	call->rc_instance = instance;
	member = rpc_instance_find_member(instance,
	    call->rc_interface, call->rc_method_name);

	if (member == NULL || member->rim_type != RPC_MEMBER_METHOD) {
		call->rc_err = rpc_error_create(ENOENT, "Member not found",
		    NULL);
		return (-1);
	}

	call->rc_if_method = &member->rim_method;
	g_thread_pool_push(context->rcx_threadpool, call, &err);
	if (err != NULL) {
		call->rc_err = rpc_error_create(EFAULT, "Cannot submit call",
		    NULL);
		return (-1);
	}

	return (0);
}

rpc_instance_t
rpc_context_find_instance(rpc_context_t context, const char *path)
{

	if (context == NULL)
		return (NULL);
	return ((path == NULL) ? context->rcx_root :
	    (g_hash_table_lookup(context->rcx_instances, path)));
}

rpc_instance_t
rpc_context_get_root(rpc_context_t context)
{

	return (context->rcx_root);
}

void
rpc_instance_emit_event(rpc_instance_t instance, const char *interface,
    const char *name, rpc_object_t args)
{
	g_mutex_lock(&instance->ri_mtx);

	if (instance->ri_context)
		rpc_context_emit_event(instance->ri_context, instance->ri_path,
		    interface, name, args);
	else
		rpc_release(args);

	g_mutex_unlock(&instance->ri_mtx);
}

int
rpc_instance_register_property(rpc_instance_t instance, const char *interface,
    const char *name, void *arg, rpc_property_getter_t getter,
    rpc_property_setter_t setter)
{
	struct rpc_if_member member;

	member.rim_name = name;
	member.rim_type = RPC_MEMBER_PROPERTY;
	member.rim_property.rp_getter = getter;
	member.rim_property.rp_setter = setter;
	member.rim_property.rp_arg = arg;

	return (rpc_instance_register_member(instance, interface, &member));
}

int
rpc_context_register_instance(rpc_context_t context, rpc_instance_t instance)
{
	GHashTableIter iter;
	rpc_object_t payload;
	rpc_object_t ifaces;
	const char *key;

	g_rw_lock_writer_lock(&context->rcx_rwlock);

	if (g_hash_table_contains(context->rcx_instances, instance->ri_path)) {
		rpc_set_last_error(EEXIST, "Instance already exists", NULL);
		g_rw_lock_writer_unlock(&context->rcx_rwlock);
		return (-1);
	}

	ifaces = rpc_array_create();
	g_hash_table_iter_init(&iter, instance->ri_interfaces);
	while (g_hash_table_iter_next(&iter, (gpointer)&key, NULL))
		rpc_array_append_stolen_value(ifaces, rpc_string_create(key));

	payload = rpc_object_pack("{s,v}",
	    "path", instance->ri_path,
	    "interfaces", ifaces);

	rpc_context_emit_event(context, "/", RPC_DISCOVERABLE_INTERFACE,
	    "instance_added", payload);

	instance->ri_context = context;

	g_hash_table_insert(context->rcx_instances, instance->ri_path, instance);
	g_rw_lock_writer_unlock(&context->rcx_rwlock);
	return (0);
}

void
rpc_context_unregister_instance(rpc_context_t context, const char *path)
{

	g_rw_lock_writer_lock(&context->rcx_rwlock);

	if (g_hash_table_remove(context->rcx_instances, path)) {
		rpc_context_emit_event(context, "/",
		    RPC_DISCOVERABLE_INTERFACE, "instance_removed",
		    rpc_string_create(path));
	}

	g_rw_lock_writer_unlock(&context->rcx_rwlock);
}

int
rpc_context_register_member(rpc_context_t context, const char *interface,
    struct rpc_if_member *m)
{

	return (rpc_instance_register_member(context->rcx_root, interface, m));
}

int
rpc_context_register_block(rpc_context_t context, const char *interface,
    const char *name, void *arg, rpc_function_t func)
{

	return (rpc_instance_register_block(context->rcx_root, interface,
	    name, arg, func));
}

int
rpc_context_register_func(rpc_context_t context, const char *interface,
    const char *name, void *arg, rpc_function_f func)
{
	rpc_function_t fn = ^(void *cookie, rpc_object_t args) {
		return (func(cookie, args));
	};

	return (rpc_context_register_block(context, interface, name, arg, fn));
}

int
rpc_context_unregister_member(rpc_context_t context, const char *interface,
    const char *name)
{

	return (rpc_instance_unregister_member(context->rcx_root, interface,
	    name));
}

void
rpc_context_emit_event(rpc_context_t context, const char *path,
    const char *interface, const char *name, rpc_object_t args)
{
	rpc_server_t server;

	g_rw_lock_reader_lock(&context->rcx_server_rwlock);
	for (guint i = 0; i < context->rcx_servers->len; i++) {
		server = g_ptr_array_index(context->rcx_servers, i);
		rpc_server_broadcast_event(server, path, interface, name,
		    args);
	}
	g_rw_lock_reader_unlock(&context->rcx_server_rwlock);

	rpc_release(args);
}

inline void *
rpc_function_get_arg(void *cookie)
{
	struct rpc_call *call = cookie;

	return (call->rc_m_arg);
}

inline rpc_connection_t
rpc_function_get_connection(void *cookie)
{
	struct rpc_call *call = cookie;

	return (rpc_connection_is_open(call->rc_conn) ? call->rc_conn : NULL);
}

inline rpc_context_t
rpc_function_get_context(void *cookie)
{
	struct rpc_call *call = cookie;

	return (call->rc_context);
}

inline rpc_instance_t
rpc_function_get_instance(void *cookie)
{
	struct rpc_call *call = cookie;

	return (call->rc_instance);
}

const char *
rpc_function_get_name(void *cookie)
{
	struct rpc_call *call = cookie;

	return (call->rc_method_name);
}

const char *
rpc_function_get_path(void *cookie)
{
	struct rpc_call *call = cookie;

	return (call->rc_path);
}

const char *
rpc_function_get_interface(void *cookie)
{
	struct rpc_call *call = cookie;

	return (call->rc_interface);
}

void
rpc_function_respond(void *cookie, rpc_object_t object)
{
	struct rpc_call *call = cookie;

	g_assert(call->rc_type == RPC_INBOUND_CALL);
	if (!call->rc_responded)
		rpc_connection_send_response(call->rc_conn,
		    call->rc_id, object);
	rpc_connection_close_inbound_call(call);
}

void
rpc_function_error(void *cookie, int code, const char *message, ...)
{
	struct rpc_call *call = cookie;
	char *msg;
	va_list ap;

	va_start(ap, message);
	g_vasprintf(&msg, message, ap);
	va_end(ap);
	rpc_connection_send_err(call->rc_conn, call->rc_id, code, msg);
	call->rc_responded = true;
	g_free(msg);
}

void
rpc_function_error_ex(void *cookie, rpc_object_t exception)
{
	struct rpc_call *call = cookie;

	rpc_connection_send_errx(call->rc_conn, call->rc_id, exception);
	call->rc_responded = true;
}

int
rpc_function_yield(void *cookie, rpc_object_t fragment)
{
	struct rpc_call *call = cookie;
	struct rpc_context *context = call->rc_context;

	g_mutex_lock(&call->rc_mtx);

	while (call->rc_producer_seqno == call->rc_consumer_seqno &&
	    !call->rc_aborted)
		g_cond_wait(&call->rc_cv, &call->rc_mtx);

	if (call->rc_aborted) {
		if (!call->rc_ended) {
			rpc_function_error(call, ECONNRESET,
			    "Call aborted");
			call->rc_ended = true;
		}

		g_mutex_unlock(&call->rc_mtx);
		rpc_release(fragment);
		return (-1);
	}

	if (context->rcx_pre_call_hook != NULL) {

	}
	rpc_connection_send_fragment(call->rc_conn, call->rc_id,
	    call->rc_producer_seqno, fragment);

	call->rc_producer_seqno++;
	call->rc_streaming = true;
	g_mutex_unlock(&call->rc_mtx);
	return (0);
}

void
rpc_function_end(void *cookie)
{
	struct rpc_call *call = cookie;

	g_mutex_lock(&call->rc_mtx);

	while (call->rc_producer_seqno == call->rc_consumer_seqno &&
	    !call->rc_aborted)
		g_cond_wait(&call->rc_cv, &call->rc_mtx);

	if (call->rc_aborted) {
		if (!call->rc_ended) {
			rpc_function_error(call, ECONNRESET,
                            "Call aborted");
			call->rc_ended = true;
		}

		g_mutex_unlock(&call->rc_mtx);
		return;
	}

	if (!call->rc_ended)
		rpc_connection_send_end(call->rc_conn, call->rc_id,
		    call->rc_producer_seqno);

	call->rc_producer_seqno++;
	call->rc_streaming = true;
	call->rc_ended = true;
	g_mutex_unlock(&call->rc_mtx);
	rpc_connection_close_inbound_call(call);
}

void
rpc_function_kill(void *cookie)
{
	struct rpc_call *call = cookie;

	g_mutex_lock(&call->rc_mtx);
	call->rc_aborted = true;
	g_cond_broadcast(&call->rc_cv);
	g_mutex_unlock(&call->rc_mtx);
}

bool
rpc_function_should_abort(void *cookie)
{
	struct rpc_call *call = cookie;

	return (call->rc_aborted);
}

void rpc_function_set_async_abort_handler(void *_Nonnull cookie,
    _Nullable rpc_abort_handler_t handler)
{
	struct rpc_call *call = cookie;

	if (call->rc_abort_handler != NULL)
		Block_release(call->rc_abort_handler);

	call->rc_abort_handler = Block_copy(handler);
}

rpc_instance_t
rpc_instance_new(void *arg, const char *fmt, ...)
{
	va_list ap;
	rpc_instance_t result;
	char *path;

	va_start(ap, fmt);
	path = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	if (!rpc_context_path_is_valid(path)) {
		rpc_set_last_error(EINVAL, "Invalid path", NULL);
		return (NULL);
	}

	result = g_malloc0(sizeof(*result));
	g_mutex_init(&result->ri_mtx);
	g_rw_lock_init(&result->ri_rwlock);
	result->ri_path = g_strdup(path);
	result->ri_subscriptions = g_hash_table_new(g_str_hash, g_str_equal);
	result->ri_interfaces = g_hash_table_new(g_str_hash, g_str_equal);
	result->ri_arg = arg;

	rpc_instance_register_interface(result, RPC_DISCOVERABLE_INTERFACE,
	    rpc_discoverable_vtable, NULL);

	rpc_instance_register_interface(result, RPC_INTROSPECTABLE_INTERFACE,
	    rpc_introspectable_vtable, NULL);

	rpc_instance_register_interface(result, RPC_DEFAULT_INTERFACE,
	    NULL, NULL);

	rpc_instance_register_interface(result, RPC_OBSERVABLE_INTERFACE,
	    rpc_observable_vtable, NULL);

	return (result);
}

void
rpc_instance_set_description(rpc_instance_t instance, const char *fmt, ...)
{
	va_list ap;
	char *description;

	va_start(ap, fmt);
	description = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	instance->ri_descr = description;
}

void *
rpc_instance_get_arg(rpc_instance_t instance)
{

	g_assert_nonnull(instance);

	return (instance->ri_arg);
}

const char *
rpc_instance_get_path(rpc_instance_t instance)
{

	g_assert_nonnull(instance);

	return (instance->ri_path);
}

void
rpc_instance_free(rpc_instance_t instance)
{
	g_assert_nonnull(instance);

	g_free(instance->ri_path);
	g_hash_table_destroy(instance->ri_interfaces);
	g_free(instance);
}

struct rpc_if_member *
rpc_instance_find_member(rpc_instance_t instance, const char *interface,
    const char *name)
{
	struct rpc_interface_priv *iface;
	struct rpc_if_member *result;

	if (name == NULL)
		return (NULL);

        if (interface == NULL)
                interface = RPC_DEFAULT_INTERFACE;

	iface = g_hash_table_lookup(instance->ri_interfaces, interface);
	if (iface == NULL) {
		debugf("member %s not found on %s\n", name,
		    interface);
		return (NULL);
	}

	g_mutex_lock(&iface->rip_mtx);
	result = g_hash_table_lookup(iface->rip_members, name);
	g_mutex_unlock(&iface->rip_mtx);

	return (result);
}

bool
rpc_instance_has_interface(rpc_instance_t instance, const char *interface)
{

	g_assert_nonnull(interface);

	return ((bool)g_hash_table_contains(instance->ri_interfaces, interface));
}

int
rpc_instance_register_interface(rpc_instance_t instance,
    const char *interface, const struct rpc_if_member *vtable, void *arg)
{
	struct rpc_interface_priv *priv;
	const struct rpc_if_member *member;

	g_assert_nonnull(interface);

	if (rpc_instance_has_interface(instance, interface))
		return (0);

	priv = g_malloc0(sizeof(*priv));
	g_mutex_init(&priv->rip_mtx);
	priv->rip_members = g_hash_table_new(g_str_hash, g_str_equal);
	priv->rip_arg = arg;
	priv->rip_name = g_strdup(interface);

	g_rw_lock_writer_lock(&instance->ri_rwlock);
	g_hash_table_insert(instance->ri_interfaces, priv->rip_name, priv);
	g_rw_lock_writer_unlock(&instance->ri_rwlock);

	rpc_instance_emit_event(instance, RPC_INTROSPECTABLE_INTERFACE,
	    "interface_added", rpc_string_create(interface));

	if (vtable == NULL)
		return (0);

	for (member = &vtable[0]; member->rim_name != NULL; member++)
		rpc_instance_register_member(instance, interface, member);

	return (0);
}

void
rpc_instance_unregister_interface(rpc_instance_t instance,
    const char *interface)
{
	g_rw_lock_writer_lock(&instance->ri_rwlock);
	g_hash_table_remove(instance->ri_interfaces, interface);
	g_rw_lock_writer_unlock(&instance->ri_rwlock);

	rpc_instance_emit_event(instance, RPC_INTROSPECTABLE_INTERFACE,
	    "interface_removed", rpc_string_create(interface));
}

int rpc_instance_register_member(rpc_instance_t instance, const char *interface,
    const struct rpc_if_member *member)
{
	struct rpc_interface_priv *priv;
	struct rpc_if_member *copy;

	if (interface == NULL)
		interface = RPC_DEFAULT_INTERFACE;

	priv = g_hash_table_lookup(instance->ri_interfaces, interface);
	if (priv == NULL) {
		rpc_set_last_error(ENOENT, "Interface not found", NULL);
		return (-1);
	}

	copy = g_memdup(member, sizeof(*member));
	copy->rim_name = g_strdup(member->rim_name);

	if (copy->rim_type == RPC_MEMBER_METHOD) {
		copy->rim_method.rm_block = Block_copy(
		    copy->rim_method.rm_block);

		if (copy->rim_method.rm_arg == NULL)
			copy->rim_method.rm_arg = priv->rip_arg;
	}

	if (copy->rim_type == RPC_MEMBER_PROPERTY) {
		if (copy->rim_property.rp_getter != NULL) {
			copy->rim_property.rp_getter = Block_copy(
			    copy->rim_property.rp_getter);
		}

		if (copy->rim_property.rp_setter != NULL) {
			copy->rim_property.rp_setter = Block_copy(
			    copy->rim_property.rp_setter);
		}

		if (copy->rim_property.rp_arg == NULL)
			copy->rim_property.rp_arg = priv->rip_arg;

		/* Emit property added event */
		rpc_instance_emit_event(instance, RPC_OBSERVABLE_INTERFACE,
		    "property_added", rpc_object_pack("{s,b,b}",
		        "name", member->rim_name,
		        "readable", member->rim_property.rp_getter != NULL,
		        "writable", member->rim_property.rp_setter != NULL));

	}

	g_mutex_lock(&priv->rip_mtx);
	g_hash_table_insert(priv->rip_members, g_strdup(member->rim_name), copy);
	g_mutex_unlock(&priv->rip_mtx);
	return (0);
}

int
rpc_instance_register_block(rpc_instance_t instance, const char *interface,
    const char *name, void *arg, rpc_function_t func)
{
	struct rpc_if_member member;

	member.rim_name = name;
	member.rim_type = RPC_MEMBER_METHOD;
	member.rim_method.rm_block = func;
	member.rim_method.rm_arg = arg;

	return (rpc_instance_register_member(instance, interface, &member));
}

int
rpc_instance_register_func(rpc_instance_t instance, const char *interface,
    const char *name, void *arg, rpc_function_f func)
{
	return (rpc_instance_register_block(instance, interface, name, arg,
	    RPC_FUNCTION(func)));
}

int
rpc_instance_unregister_member(rpc_instance_t instance, const char *interface,
    const char *name)
{
	struct rpc_interface_priv *priv;
	struct rpc_if_member *member;

        g_assert_nonnull(name);

        if (interface == NULL)
                interface = RPC_DEFAULT_INTERFACE;

	priv = g_hash_table_lookup(instance->ri_interfaces, interface);
	if (priv == NULL) {
		rpc_set_last_error(ENOENT, "Interface not found", NULL);
		return (-1);
	}

	g_mutex_lock(&priv->rip_mtx);
	member = g_hash_table_lookup(priv->rip_members, name);
	if (member == NULL) {
		rpc_set_last_error(ENOENT, "Member not found", NULL);
		return (-1);
	}

	if (member->rim_type == RPC_MEMBER_PROPERTY)
		rpc_instance_emit_event(instance, "librpc.Observable",
		    "property_removed", rpc_object_pack("{s}", "name", name));
	else if (member->rim_type == RPC_MEMBER_METHOD) {
		Block_release(member->rim_method.rm_block);
		g_free((void *)member->rim_name);
		g_free(member);
	}

	g_hash_table_remove(priv->rip_members, name);
	g_mutex_unlock(&priv->rip_mtx);

	debugf("unregistered %s", name);
	return (0);
}

void
rpc_context_set_pre_call_hook(rpc_context_t context, rpc_function_t fn)
{

	context->rcx_pre_call_hook = fn;
}


void
rpc_context_set_post_call_hook(rpc_context_t context, rpc_function_t fn)
{

	context->rcx_post_call_hook = fn;
}

int
rpc_instance_get_property_rights(rpc_instance_t instance, const char *interface,
    const char *name)
{
	struct rpc_if_member *member;
	int rights = 0;

	member = rpc_instance_find_member(instance, interface, name);
	if (member == NULL || member->rim_type != RPC_MEMBER_PROPERTY) {
		rpc_set_last_error(ENOENT, "Property not found", NULL);
		return (-1);
	}

	if (member->rim_property.rp_getter != NULL)
		rights |= RPC_PROPERTY_READ;

	if (member->rim_property.rp_setter != NULL)
		rights |= RPC_PROPERTY_WRITE;

	return (rights);
}

void
rpc_instance_property_changed(rpc_instance_t instance, const char *interface,
    const char *name, rpc_object_t value)
{
	struct rpc_if_member *prop;
	struct rpc_property_cookie cookie;
	bool release = false;

	prop = rpc_instance_find_member(instance, interface, name);
	g_assert(prop != NULL);
	g_assert(prop->rim_type == RPC_MEMBER_PROPERTY);

	if (value == NULL) {
		cookie.instance = instance;
		cookie.name = name;
		cookie.arg = prop->rim_property.rp_arg;
		cookie.error = NULL;
		value = prop->rim_property.rp_getter(&cookie);

		if (cookie.error != NULL)
			return;

		release = true;
	}

	rpc_instance_emit_event(instance, RPC_OBSERVABLE_INTERFACE, "changed",
	    rpc_object_pack("{s,s,v}",
	        "interface", interface,
	        "name", name,
	        "value", rpc_retain(value)));

	if (release)
		rpc_release(value);
}

rpc_instance_t
rpc_property_get_instance(void *cookie)
{
	struct rpc_property_cookie *prop = cookie;

	return (prop->instance);
}

void *
rpc_property_get_arg(void *cookie)
{
	struct rpc_property_cookie *prop = cookie;

	return (prop->arg);
}

const char *
rpc_property_get_name(void *cookie)
{
	struct rpc_property_cookie *prop = cookie;

	return (prop->name);
}

void
rpc_property_error(void *cookie, int code, const char *fmt, ...)
{
	struct rpc_property_cookie *prop = cookie;
	char *msg;
	va_list ap;

	va_start(ap, fmt);
	msg = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	prop->error = rpc_error_create(code, msg, NULL);
	g_free(msg);

}

static rpc_object_t
rpc_get_objects(void *cookie, rpc_object_t args __unused)
{
	rpc_context_t context = rpc_function_get_context(cookie);
	GHashTableIter iter;
	const char *k;
	char *prefix = NULL;
	rpc_instance_t instance;
	rpc_instance_t v;
	rpc_object_t list;

	instance = rpc_function_get_instance(cookie);
	list = rpc_array_create();

	if (strlen(rpc_instance_get_path(instance)) > 1)
		prefix = g_strdup_printf("%s/", rpc_instance_get_path(instance));

	g_rw_lock_reader_lock(&context->rcx_rwlock);
	g_hash_table_iter_init(&iter, context->rcx_instances);

	while (g_hash_table_iter_next(&iter, (gpointer)&k, (gpointer)&v)) {
		if (prefix != NULL && !g_str_has_prefix(v->ri_path, prefix))
			continue;
		rpc_array_append_stolen_value(list, rpc_object_pack("{s,s}",
		    "path", v->ri_path,
		    "description", v->ri_descr));
	}

	g_rw_lock_reader_unlock(&context->rcx_rwlock);
	g_free(prefix);
	return (list);
}

static rpc_object_t
rpc_get_interfaces(void *cookie, rpc_object_t args __unused)
{
	GHashTableIter iter;
	const char *k;
	void *v;
	rpc_object_t list = rpc_array_create();
	rpc_instance_t instance = rpc_function_get_instance(cookie);

	g_rw_lock_reader_lock(&instance->ri_rwlock);
	g_hash_table_iter_init(&iter, instance->ri_interfaces);
	while (g_hash_table_iter_next(&iter, (gpointer)&k, (gpointer)&v))
		rpc_array_append_stolen_value(list, rpc_string_create(k));

	g_rw_lock_reader_unlock(&instance->ri_rwlock);
	return (list);
}

static rpc_object_t
rpc_get_methods(void *cookie, rpc_object_t args)
{
	GHashTableIter iter;
	struct rpc_interface_priv *priv;
	const char *interface;
	const char *k;
	struct rpc_if_member *v;
	rpc_object_t result = rpc_array_create();
	rpc_instance_t instance = rpc_function_get_instance(cookie);

	if (rpc_object_unpack(args, "[s]", &interface) < 1) {
		rpc_function_error(cookie, EINVAL, "Invalid arguments passed");
		return (NULL);
	}

        if (interface == NULL)
                interface = RPC_DEFAULT_INTERFACE;

	g_rw_lock_reader_lock(&instance->ri_rwlock);
	priv = g_hash_table_lookup(instance->ri_interfaces, interface);
	if (priv == NULL) {
		rpc_function_error(cookie, ENOENT, "Interface not found");
		g_rw_lock_reader_unlock(&instance->ri_rwlock);
		return (NULL);
	}

	g_hash_table_iter_init(&iter, priv->rip_members);
	while (g_hash_table_iter_next(&iter, (gpointer)&k, (gpointer)&v)) {
		if (v->rim_type != RPC_MEMBER_METHOD)
			continue;
		rpc_array_append_stolen_value(result, rpc_string_create(k));
	}

	g_rw_lock_reader_unlock(&instance->ri_rwlock);
	return (result);
}

static rpc_object_t
rpc_get_events(void *cookie, rpc_object_t args)
{
	GHashTableIter iter;
	struct rpc_interface_priv *priv;
	const char *interface;
	const char *k;
	struct rpc_if_member *v;
	rpc_object_t result = rpc_array_create();
	rpc_instance_t instance = rpc_function_get_instance(cookie);

	if (rpc_object_unpack(args, "[s]", &interface) < 1) {
		rpc_function_error(cookie, EINVAL, "Invalid arguments passed");
		return (NULL);
	}

	g_rw_lock_reader_lock(&instance->ri_rwlock);
	priv = g_hash_table_lookup(instance->ri_interfaces, interface);
	if (priv == NULL) {
		rpc_function_error(cookie, ENOENT, "Interface not found");
		g_rw_lock_reader_unlock(&instance->ri_rwlock);
		return (NULL);
	}

	g_hash_table_iter_init(&iter, priv->rip_members);
	while (g_hash_table_iter_next(&iter, (gpointer)&k, (gpointer)&v)) {
		if (v->rim_type != RPC_MEMBER_EVENT)
			continue;

		rpc_array_append_stolen_value(result, rpc_string_create(k));
	}

	g_rw_lock_reader_unlock(&instance->ri_rwlock);
	return (result);
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

static rpc_object_t
rpc_observable_property_get(void *cookie, rpc_object_t args)
{
	rpc_instance_t inst = rpc_function_get_instance(cookie);
	rpc_object_t result;
	struct rpc_property_cookie prop;
	struct rpc_if_member *member;
	const char *interface;
	const char *name;

	if (rpc_object_unpack(args, "[s,s]", &interface, &name) < 2) {
		rpc_function_error(cookie, EINVAL, "Invalid arguments passed");
		return (NULL);
	}

	member = rpc_instance_find_member(inst, interface, name);
	if (member == NULL || member->rim_type != RPC_MEMBER_PROPERTY) {
		rpc_function_error(cookie, ENOENT, "Property not found");
		return (NULL);
	}

	if (member->rim_property.rp_getter == NULL) {
		rpc_function_error(cookie, EPERM, "Property read not allowed");
		return (NULL);
	}

	prop.instance = inst;
	prop.name = name;
	prop.arg = member->rim_property.rp_arg;
	prop.error = NULL;
	result = member->rim_property.rp_getter(&prop);

	if (prop.error != NULL) {
		rpc_function_error_ex(cookie, prop.error);
		return (NULL);
	}

	return (result);
}

static rpc_object_t
rpc_observable_property_set(void *cookie, rpc_object_t args)
{
	rpc_instance_t inst = rpc_function_get_instance(cookie);
	struct rpc_property_cookie prop;
	struct rpc_if_member *member;
	const char *interface;
	const char *name;
	rpc_object_t value;

	if (rpc_object_unpack(args, "[s,s,v]", &interface, &name, &value) < 3) {
		rpc_function_error(cookie, EINVAL, "Invalid arguments passed");
		return (NULL);
	}

	member = rpc_instance_find_member(inst, interface, name);
	if (member == NULL || member->rim_type != RPC_MEMBER_PROPERTY) {
		rpc_function_error(cookie, ENOENT, "Property not found");
		return (NULL);
	}

	if (member->rim_property.rp_setter == NULL) {
		rpc_function_error(cookie, EPERM, "Property write not allowed");
		return (NULL);
	}

	prop.instance = inst;
	prop.name = name;
	prop.arg = member->rim_property.rp_arg;
	prop.error = NULL;
	member->rim_property.rp_setter(&prop, value);

	if (prop.error != NULL) {
		rpc_function_error_ex(cookie, prop.error);
		return (NULL);
	}

	rpc_instance_property_changed(inst, interface, name, value);
	return (NULL);
}

static rpc_object_t
rpc_observable_property_get_all(void *cookie, rpc_object_t args)
{
	rpc_instance_t inst = rpc_function_get_instance(cookie);
	GHashTableIter iter;
	struct rpc_property_cookie prop;
	const char *interface;
	const char *k;
	struct rpc_if_member *v;
	struct rpc_interface_priv *priv;
	rpc_object_t value;
	rpc_object_t result;
	rpc_object_t item;

	if (rpc_object_unpack(args, "[s]", &interface) < 1) {
		rpc_function_error(cookie, EINVAL, "Invalid arguments passed");
		return (NULL);
	}

	priv = g_hash_table_lookup(inst->ri_interfaces, interface);
	if (priv == NULL) {
		rpc_function_error(cookie, ENOENT, "Interface not found");
		return (NULL);
	}

	result = rpc_array_create();
	g_mutex_lock(&priv->rip_mtx);
	g_hash_table_iter_init(&iter, priv->rip_members);

	while (g_hash_table_iter_next(&iter, (gpointer)&k, (gpointer)&v)) {
		if (v->rim_type != RPC_MEMBER_PROPERTY)
			continue;

		if (v->rim_property.rp_getter == NULL) {
			value = rpc_error_create(EPERM, "Not readable", NULL);
			item = rpc_object_pack(
			    "<com.twoporeguys.librpc.PropertyDescriptor>{s,v}",
			    "name", v->rim_name,
			    "value", value);

			rpc_array_append_stolen_value(result, item);
			continue;
		}

		prop.instance = inst;
		prop.name = k;
		prop.arg = v->rim_property.rp_arg;
		prop.error = NULL;
		value = v->rim_property.rp_getter(&prop);

		if (prop.error != NULL)
			value = prop.error;

		item = rpc_object_pack(
		    "<com.twoporeguys.librpc.PropertyDescriptor>{s,v}",
		    "name", v->rim_name,
		    "value", value);

		rpc_array_append_stolen_value(result, item);
	}

	g_mutex_unlock(&priv->rip_mtx);
	return (result);
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
