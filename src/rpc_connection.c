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

#include <stdlib.h>
#include <rpc/object.h>
#include <rpc/connection.h>
#include <glib.h>
#include "internal.h"
#include "linker_set.h"
#include "serializer/msgpack.h"

static const struct rpc_transport *rpc_find_transport(const char *);
static rpc_object_t rpc_new_id(void);
static rpc_object_t rpc_pack_frame(const char *, const char *, rpc_object_t,
    rpc_object_t);
static int rpc_send_frame(rpc_connection_t, rpc_object_t);
static void on_rpc_call(rpc_connection_t, rpc_object_t, rpc_object_t);
static void on_rpc_response(rpc_connection_t, rpc_object_t, rpc_object_t);
static void on_rpc_fragment(rpc_connection_t, rpc_object_t, rpc_object_t);
static void on_rpc_end(rpc_connection_t, rpc_object_t, rpc_object_t);
static void on_rpc_abort(rpc_connection_t, rpc_object_t, rpc_object_t);
static void on_rpc_error(rpc_connection_t, rpc_object_t, rpc_object_t);
static void on_events_event(rpc_connection_t, rpc_object_t, rpc_object_t);

struct message_handler {
    const char *namespace;
    const char *name;
    void (*handler)(rpc_connection_t conn, rpc_object_t args, rpc_object_t id);
};

static const struct message_handler handlers[] = {
    { "rpc", "call", on_rpc_call },
    { "rpc", "response", on_rpc_response },
    { "rpc", "fragment", on_rpc_fragment },
    { "rpc", "end", on_rpc_end },
    { "rpc", "abort", on_rpc_abort },
    { "rpc", "error", on_rpc_error },
    { "events", "event", on_events_event },
    { NULL }
};

SET_DECLARE(transport_set, struct rpc_transport);

static const struct rpc_transport *
rpc_find_transport(const char *scheme)
{
	const struct rpc_transport *t;
	int i;

	SET_FOREACH(t, transport_set) {
		for (i = 0; t->schemas[i] != NULL; i++) {
			if (!g_strcmp0(t->schemas[i], scheme))
				return (t);
		}
	}

	return (NULL);
}

static void
rpc_serialize_fds(rpc_object_t obj, int *fds, size_t *nfds)
{

}

static void
rpc_restore_fds(rpc_object_t obj, int *fds, size_t nfds)
{

}

static rpc_object_t
rpc_pack_frame(const char *ns, const char *name, rpc_object_t id,
    rpc_object_t args)
{
	rpc_object_t obj;

	obj = rpc_dictionary_create(NULL, NULL, 0);
	rpc_dictionary_set_string(obj, "namespace", ns);
	rpc_dictionary_set_string(obj, "name", name);
	rpc_dictionary_set_value(obj, "id", id);
	rpc_dictionary_set_value(obj, "args", args);
	return (obj);
}

static void
on_rpc_call(rpc_connection_t conn, rpc_object_t args, rpc_object_t id)
{
	struct rpc_inbound_call *call;

	call = calloc(1, sizeof(*call));
	call->rc_id = id;
	call->rc_args = rpc_dictionary_get_value(args, "args");
	call->rc_method = rpc_dictionary_get_string(args, "method");
	g_mutex_init(&call->rc_mtx);
	g_cond_init(&call->rc_cv);
}

static void
on_rpc_response(rpc_connection_t conn, rpc_object_t args, rpc_object_t id)
{
	rpc_call_t call;

	call = g_hash_table_lookup(conn->rco_calls,
	    rpc_string_get_string_ptr(id));
	if (call == NULL) {
		return;
	}

	g_mutex_lock(&call->rc_mtx);
	call->rc_status = RPC_CALL_DONE;
	call->rc_result = args;

	g_cond_broadcast(&call->rc_cv);
	g_mutex_unlock(&call->rc_mtx);
}

static void
on_rpc_fragment(rpc_connection_t conn, rpc_object_t args, rpc_object_t id)
{

}

static void
on_rpc_end(rpc_connection_t conn, rpc_object_t args, rpc_object_t id)
{

}

static void
on_rpc_abort(rpc_connection_t conn, rpc_object_t args, rpc_object_t id)
{

}

static void
on_rpc_error(rpc_connection_t conn, rpc_object_t args, rpc_object_t id)
{

}

static void
on_events_event(rpc_connection_t conn, rpc_object_t args, rpc_object_t id)
{

}

struct rpc_call *
rpc_call_alloc(rpc_connection_t conn, rpc_object_t id)
{
	struct rpc_call *call;

	call = calloc(1, sizeof(*call));
	call->rc_conn = conn;
	call->rc_id = id != NULL ? id : rpc_new_id();
	g_mutex_init(&call->rc_mtx);
	g_cond_init(&call->rc_cv);
}

static int
rpc_send_frame(rpc_connection_t conn, rpc_object_t frame)
{
	void *buf;
	size_t len;

	if (rpc_msgpack_serialize(frame, &buf, &len) != 0)
		return (-1);

	return (conn->rco_transport->send_msg(conn->rco_transport_arg, )
}

void
rpc_call_internal(rpc_connection_t conn, rpc_object_t args)
{
	struct rpc_call *call;


}

static void
rpc_answer_call(rpc_call_t call)
{
	if (call->rc_callback != NULL) {
		;
	}


}

static rpc_object_t
rpc_new_id(void)
{
	char *str = g_uuid_string_random();
	rpc_object_t ret;

	ret = rpc_string_create(str);
	g_free(str);
	return (ret);
}

rpc_connection_t
rpc_connection_create(const char *uri, int flags)
{
	char *scheme;

	scheme = g_uri_parse_scheme(uri);

}

int
rpc_connection_close(rpc_connection_t conn)
{

}

void
rpc_connection_dispatch(rpc_connection_t conn, rpc_object_t frame)
{
	rpc_object_t id;
	const struct message_handler *h;
	const char *namespace;
	const char *name;

	id = rpc_dictionary_get_value(frame, "id");
	namespace = rpc_dictionary_get_string(frame, "namespace");
	name = rpc_dictionary_get_string(frame, "name");

	for (h = &handlers[0]; h->namespace != NULL; h++) {
		if (g_strcmp0(namespace, h->namespace))
			continue;

		if (g_strcmp0(name, h->name))
			continue;

		h->handler(conn, rpc_dictionary_get_value(frame, "args"), id);
		return;
	}


}

int
rpc_connection_login_user(rpc_connection_t conn, const char *username,
    const char *password)
{

}

int
rpc_connection_login_service(rpc_connection_t conn, const char *name)
{

}

int
rpc_connection_subscribe_event(rpc_connection_t conn, const char *name)
{

}

int
rpc_connection_unsubscribe_event(rpc_connection_t conn, const char *name)
{

}

int
rpc_connection_call_sync(rpc_connection_t conn, const char *method, ...)
{

}

rpc_call_t
rpc_connection_call(rpc_connection_t conn, const char *name, rpc_object_t args)
{
	struct rpc_call *call;
	rpc_object_t id = rpc_new_id();
	rpc_object_t msg;

	call = rpc_call_alloc(conn);
	call->rc_id = id;
	call->rc_type = "call";
	call->rc_method = name;
	call->rc_args = json_object();
	call->rc_callback = cb;
	call->rc_callback_arg = cb_arg;

	json_object_set_new(call->rc_args, "method", json_string(name));
	json_object_set(call->rc_args, "args", args);
	rpc_call_internal(conn, "call", call);

	return (call);
}

int
rpc_connection_send_event(rpc_connection_t conn, const char *name,
    rpc_object_t args)
{
	rpc_object_t frame;
	rpc_object_t event;
	const char *names[] = {"name", "args"};
	rpc_object_t values[] = {
	    rpc_string_create(name),
	    args
	};

	event = rpc_dictionary_create(names, values, 2);
	frame = rpc_pack_frame("events", "event", NULL, event);

	if (rpc_send_frame(conn, frame) != 0)
		return (-1);

	return (0);
}

void
rpc_connection_set_event_handler(rpc_connection_t conn, rpc_handler_t handler)
{

}

int
rpc_call_wait(rpc_call_t call)
{

	g_mutex_lock(&call->rc_mtx);

	while (call->rc_status == RPC_CALL_IN_PROGRESS)
		g_cond_wait(&call->rc_cv, &call->rc_mtx);

	g_mutex_unlock(&call->rc_mtx);
	return (0);
}

int
rpc_call_continue(rpc_call_t call, bool sync)
{
	rpc_object_t frame;
	int64_t seqno;

	g_mutex_lock(&call->rc_mtx);
	seqno = call->rc_seqno + 1;
	frame = rpc_pack_frame("rpc", "continue", call->rc_id,
	    rpc_int64_create(seqno));

	if ()

	call->rc_status = RPC_CALL_IN_PROGRESS;

	if (sync) {
		rpc_call_wait(call);
		g_mutex_unlock(&call->rc_mtx);
		return (rpc_call_success(call));
	}

	g_mutex_unlock(&call->rc_mtx);
	return (0);
}

int
rpc_call_abort(rpc_call_t call)
{

}

int
rpc_call_timedwait(rpc_call_t call, const struct timespec *ts)
{

}

int
rpc_call_success(rpc_call_t call)
{

}

rpc_object_t
rpc_call_result(rpc_call_t call)
{

}