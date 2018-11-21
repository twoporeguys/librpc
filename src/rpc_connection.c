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
#include <errno.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <rpc/config.h>
#include <rpc/object.h>
#include <rpc/connection.h>
#ifdef ENABLE_LIBDISPATCH
#include <dispatch/dispatch.h>
#endif
#include "linker_set.h"
#include "internal.h"
#include "notify.h"
#include "serializer/msgpack.h"

#define	DEFAULT_RPC_TIMEOUT	60
#define	MAX_FDS			128

typedef enum rpc_close_source
{
	RPC_CLOSE_CALLED,
	RPC_ABORTED,
} rpc_close_source_t;

struct work_item;

static rpc_object_t rpc_new_id(void);
static rpc_object_t rpc_pack_frame(const char *, const char *, rpc_object_t,
    rpc_object_t);
static bool rpc_run_callback(rpc_connection_t, struct work_item *);
static struct rpc_call *rpc_call_alloc(rpc_connection_t, rpc_object_t,
    const char *, const char *, const char *, rpc_object_t);
static int rpc_send_frame(rpc_connection_t, rpc_object_t);
static void on_rpc_call(rpc_connection_t, rpc_object_t, rpc_object_t);
static void on_rpc_response(rpc_connection_t, rpc_object_t, rpc_object_t);
static void on_rpc_start_stream(rpc_connection_t, rpc_object_t, rpc_object_t);
static void on_rpc_fragment(rpc_connection_t, rpc_object_t, rpc_object_t);
static void on_rpc_continue(rpc_connection_t, rpc_object_t, rpc_object_t);
static void on_rpc_end(rpc_connection_t, rpc_object_t, rpc_object_t);
static void on_rpc_abort(rpc_connection_t, rpc_object_t, rpc_object_t);
static void on_rpc_error(rpc_connection_t, rpc_object_t, rpc_object_t);
static void on_events_event(rpc_connection_t, rpc_object_t, rpc_object_t);
static void on_events_event_burst(rpc_connection_t, rpc_object_t, rpc_object_t);
static void on_events_subscribe(rpc_connection_t, rpc_object_t, rpc_object_t);
static void on_events_unsubscribe(rpc_connection_t, rpc_object_t, rpc_object_t);
static void rpc_callback_worker(void *, void *);
static inline rpc_call_status_t rpc_call_status_locked(rpc_call_t);
static int rpc_call_wait_locked(rpc_call_t);
static gboolean rpc_call_timeout(gpointer user_data);
static struct rpc_subscription *rpc_connection_subscribe_event_locked(
    rpc_connection_t, const char *, const char *, const char *);
static struct rpc_subscription *rpc_connection_find_subscription(rpc_connection_t,
    const char *, const char *, const char *);
static void rpc_connection_free_resources(rpc_connection_t);
static int cancel_timeout_locked(rpc_call_t call);
static void rpc_connection_set_default_fn_handlers(rpc_connection_t);
static inline rpc_object_t rpc_call_result_save(rpc_call_t call);
static int rpc_connection_do_close(rpc_connection_t conn, rpc_close_source_t);
static rpc_connection_t rpc_connection_init(void);
static void rpc_abort_worker(void *arg, void *data);
static void call_abort_locked(struct rpc_call *call);
static void rpc_subscription_release(struct rpc_subscription *sub);
static void rpc_rsh_release(struct rpc_subscription_handler *rsh);
static bool rpc_connection_retain_if_open(rpc_connection_t conn);
static int rpc_connection_unsubscribe_event_locked(rpc_connection_t conn,
    struct rpc_subscription *sub);

struct message_handler
{
	const char *namespace;
	const char *name;
	void (*handler)(rpc_connection_t, rpc_object_t, rpc_object_t);
};

struct queue_item
{
	rpc_call_status_t status;
	rpc_object_t item;
};

struct work_item
{
    	rpc_call_t call;
    	rpc_object_t event;
};

static const struct message_handler handlers[] = {
	{ "rpc", "call", on_rpc_call },
	{ "rpc", "response", on_rpc_response },
	{ "rpc", "start_stream", on_rpc_start_stream },
	{ "rpc", "fragment", on_rpc_fragment },
	{ "rpc", "continue", on_rpc_continue },
	{ "rpc", "end", on_rpc_end },
	{ "rpc", "abort", on_rpc_abort },
	{ "rpc", "error", on_rpc_error },
	{ "events", "event", on_events_event },
	{ "events", "event_burst", on_events_event_burst },
	{ "events", "subscribe", on_events_subscribe },
	{ "events", "unsubscribe", on_events_unsubscribe },
	{ }
};


static size_t
rpc_serialize_fds(rpc_object_t obj, int *fds, size_t *nfds, size_t idx)
{
	__block size_t counter = idx;

	switch (rpc_get_type(obj)) {
	case RPC_TYPE_FD:
		fds[counter] = obj->ro_value.rv_fd;
		obj->ro_value.rv_fd = (int)counter;
		counter++;
		break;

#if defined(__linux__)
	case RPC_TYPE_SHMEM:
		fds[counter] = obj->ro_value.rv_shmem.rsb_fd;
		obj->ro_value.rv_shmem.rsb_fd = (int)counter;
		counter++;
		break;
#endif

	case RPC_TYPE_ARRAY:
		rpc_array_apply(obj, ^(size_t aidx __unused, rpc_object_t i) {
			counter += rpc_serialize_fds(i, fds, nfds, idx);
			return ((bool)true);
		});
		break;

	case RPC_TYPE_DICTIONARY:
		rpc_dictionary_apply(obj, ^(const char *name __unused,
		    rpc_object_t i) {
			counter += rpc_serialize_fds(i, fds, nfds, idx);
			return ((bool)true);
		});
		break;

	default:
		break;
	}

	return (counter);
}

static void
rpc_restore_fds(rpc_object_t obj, int *fds, size_t nfds)
{

	switch (rpc_get_type(obj)) {
		case RPC_TYPE_FD:
			obj->ro_value.rv_fd = fds[obj->ro_value.rv_fd];
			break;

#if defined(__linux__)
		case RPC_TYPE_SHMEM:
			obj->ro_value.rv_shmem.rsb_fd =
			    fds[obj->ro_value.rv_shmem.rsb_fd];
			break;
#endif

		case RPC_TYPE_ARRAY:
			rpc_array_apply(obj, ^(size_t idx __unused,
			    rpc_object_t item) {
				rpc_restore_fds(item, fds, nfds);
				return ((bool)true);
			});
			break;

		case RPC_TYPE_DICTIONARY:
			rpc_dictionary_apply(obj, ^(const char *key __unused,
			    rpc_object_t value) {
				rpc_restore_fds(value, fds, nfds);
				return ((bool)true);
			});
			break;

		default:
			break;
	}
}

static bool
rpc_run_callback(rpc_connection_t conn, struct work_item *item)
{
	GError *err = NULL;

	/* must be called with connection retained */
#ifdef ENABLE_LIBDISPATCH
	if (conn->rco_dispatch_queue != NULL) {
		dispatch_async(conn->rco_dispatch_queue, ^{
			rpc_callback_worker(item, conn);
		});

		return (true);
	}
#endif
	g_thread_pool_push(conn->rco_callback_pool, item, &err);
	if (err != NULL) {
		g_error_free(err);
		return (false);
	}

	return (true);
}

static void
rpc_callback_worker(void *arg, void *data)
{
	struct work_item *item = arg;
	struct rpc_subscription *sub;
	struct rpc_subscription_handler *handler;
	const char *path;
	const char *interface;
	const char *name;
	rpc_call_t call;
	rpc_connection_t conn = data;
	rpc_call_status_t call_status;
	bool ret;


	if (!rpc_connection_retain_if_open(conn))
		return;

	if (item->call) {
		call = item->call;

		if (call->rc_callback == NULL)
			goto done;

		ret = call->rc_callback(call);

		call_status = rpc_call_status(call);
		if (call_status == RPC_CALL_MORE_AVAILABLE ||
		    call_status == RPC_CALL_STREAM_START) {
			if (ret)
				rpc_call_continue(call, false);
			else
				rpc_call_abort(call);
		}

		goto done;
	}

	if (item->event) {

		path = rpc_dictionary_get_string(item->event, "path");
		interface = rpc_dictionary_get_string(item->event, "interface");
		name = rpc_dictionary_get_string(item->event, "name");
		data = rpc_dictionary_get_value(item->event, "args");
		g_rw_lock_writer_lock(&conn->rco_subscription_rwlock);
		sub = rpc_connection_find_subscription(conn, path, interface, name);

		if (sub != NULL) {
			if (sub->rsu_busy) {
				rpc_run_callback(conn, item);
				g_rw_lock_writer_unlock(&conn->rco_subscription_rwlock);
				rpc_connection_release(conn);
				return;
			}
			sub->rsu_busy = true;
			for (guint i = 0; i < sub->rsu_handlers->len; i++) {
				handler = g_ptr_array_index(sub->rsu_handlers, i);
				g_rw_lock_writer_unlock(&conn->rco_subscription_rwlock);
				handler->rsh_handler(path, interface, name, data);
				g_rw_lock_writer_lock(&conn->rco_subscription_rwlock);
			}
			sub->rsu_busy = false;
		}
		g_rw_lock_writer_unlock(&conn->rco_subscription_rwlock);

		if (conn->rco_event_handler != NULL)
			conn->rco_event_handler(path, interface, name, data);

		rpc_release(item->event);
	}

done:
	rpc_connection_release(conn);
	g_free(item);
}

static rpc_object_t
rpc_pack_frame(const char *ns, const char *name, rpc_object_t id,
    rpc_object_t args)
{
	rpc_object_t obj;

	obj = rpc_dictionary_create();
	rpc_dictionary_set_string(obj, "namespace", ns);
	rpc_dictionary_set_string(obj, "name", name);
	rpc_dictionary_steal_value(obj, "id", id ? rpc_retain(id) : rpc_null_create());
	rpc_dictionary_steal_value(obj, "args", args);
	return (obj);
}

rpc_context_t
rpc_connection_get_context(rpc_connection_t conn)
{
	return (conn->rco_rpc_context);
}

int
rpc_connection_set_context(rpc_connection_t conn, rpc_context_t ctx)
{

	g_mutex_lock(&conn->rco_mtx);
	if (!rpc_connection_is_open(conn) || (conn->rco_rpc_context != NULL)) {
		rpc_set_last_errorf(EINVAL, "%s",
		    (conn->rco_rpc_context != NULL) ? "Context already set" :
		    "Connection not open");
		g_mutex_unlock(&conn->rco_mtx);
		return (-1);
	}
	conn->rco_rpc_context = ctx;
	g_mutex_unlock(&conn->rco_mtx);
	return (0);
}

static int
cancel_timeout_locked(rpc_call_t call)
{
	/* Cancel timeout source */
	if (call->rc_timeout != NULL) {
		if (call->rc_timedout)
			return (-1);

		g_source_destroy(call->rc_timeout);
		g_source_unref(call->rc_timeout);
		call->rc_timeout = NULL;
	}
	return (0);
}

static void
on_rpc_call(rpc_connection_t conn, rpc_object_t args, rpc_object_t id)
{
	rpc_call_t call;
	const char *method = NULL;
	const char *interface = NULL;
	const char *path = NULL;
	rpc_object_t call_args = NULL;
	rpc_object_t err;
	int res;

	if (conn->rco_rpc_context == NULL) {
		rpc_connection_send_err(conn, id, ENOTSUP, "Not supported");
		return;
	}

	rpc_object_unpack(args, "{s,s,s,v}",
	    "method", &method,
	    "interface", &interface,
	    "path", &path,
	    "args", &call_args);

	rpc_retain(id);
	call = rpc_call_alloc(conn, id, path, interface, method, call_args);
	if (call == NULL) {
		err = rpc_get_last_error();
		rpc_connection_send_err(conn, id, rpc_error_get_code(err),
		    rpc_error_get_message(err));
		return;
	}

	call->rc_type = RPC_INBOUND_CALL;

	g_rw_lock_writer_lock(&conn->rco_icall_rwlock);
	g_hash_table_insert(conn->rco_inbound_calls,
	    (gpointer)rpc_string_get_string_ptr(id), call);
	g_rw_lock_writer_unlock(&conn->rco_icall_rwlock);

	if (conn->rco_server != NULL)
		res = rpc_server_dispatch(conn->rco_server, call);
	else
		res = rpc_context_dispatch(conn->rco_rpc_context, call);

	if (res != 0) {
		if (call->rc_err != NULL) {
			rpc_function_error(call,
			    rpc_error_get_code(call->rc_err),
			    rpc_error_get_message(call->rc_err));
		}
		rpc_connection_close_inbound_call(call);
	}
}

static void
on_rpc_response(rpc_connection_t conn, rpc_object_t args, rpc_object_t id)
{
	struct queue_item *q_item;
	struct work_item *item;
	rpc_call_t call;

	g_rw_lock_reader_lock(&conn->rco_call_rwlock);
	call = g_hash_table_lookup(conn->rco_calls,
	    rpc_string_get_string_ptr(id));
	if (call == NULL) {
		g_rw_lock_reader_unlock(&conn->rco_call_rwlock);
		return;
	}

	g_assert(call->rc_type == RPC_OUTBOUND_CALL);

	rpc_connection_call_retain(call);
	g_mutex_lock(&call->rc_mtx);

	if (cancel_timeout_locked(call) != 0) {
		g_mutex_unlock(&call->rc_mtx);
		g_rw_lock_reader_unlock(&conn->rco_call_rwlock);
		rpc_connection_call_release(call);
		return;
	}

	g_rw_lock_reader_unlock(&conn->rco_call_rwlock);

	if (call->rc_callback) {
		item = g_malloc0(sizeof(*item));
		item->call = call;
		if (!rpc_run_callback(conn, item))
			g_free(item);
	}

	q_item = g_malloc(sizeof(*q_item));
	q_item->status = RPC_CALL_DONE;
	q_item->item = rpc_retain(args);

	g_queue_push_tail(call->rc_queue, q_item);
	notify_signal(&call->rc_notify);
	g_mutex_unlock(&call->rc_mtx);
	rpc_connection_call_release(call);
}

static void
on_rpc_start_stream(rpc_connection_t conn, rpc_object_t args, rpc_object_t id)
{
	struct queue_item *q_item;
	struct work_item *item;
	rpc_call_t call;
	int64_t seqno;

	g_rw_lock_reader_lock(&conn->rco_call_rwlock);
	call = g_hash_table_lookup(conn->rco_calls,
	    rpc_string_get_string_ptr(id));
	if (call == NULL) {
		g_rw_lock_reader_unlock(&conn->rco_call_rwlock);
		return;
	}
	rpc_connection_call_retain(call);
	g_mutex_lock(&call->rc_mtx);
	if (cancel_timeout_locked(call) != 0) {
		g_mutex_unlock(&call->rc_mtx);
		g_rw_lock_reader_unlock(&conn->rco_call_rwlock);
		rpc_connection_call_release(call);
		return;
	}

	g_rw_lock_reader_unlock(&conn->rco_call_rwlock);

	seqno = rpc_dictionary_get_int64(args, "seqno");

	if (call->rc_callback) {
		item = g_malloc0(sizeof(*item));
		item->call = call;
		if (!rpc_run_callback(conn, item))
			g_free(item);
	}

	q_item = g_malloc(sizeof(*q_item));
	q_item->status = RPC_CALL_STREAM_START;
	q_item->item = rpc_null_create();

	g_queue_push_tail(call->rc_queue, q_item);
	notify_signal(&call->rc_notify);
	g_mutex_unlock(&call->rc_mtx);
	rpc_connection_call_release(call);
}

static void
on_rpc_fragment(rpc_connection_t conn, rpc_object_t args, rpc_object_t id)
{
	struct queue_item *q_item;
	struct work_item *item;
	rpc_call_t call;
	rpc_object_t payload;
	int64_t seqno;

	g_rw_lock_reader_lock(&conn->rco_call_rwlock);
	call = g_hash_table_lookup(conn->rco_calls,
	    rpc_string_get_string_ptr(id));
	if (call == NULL) {
		g_rw_lock_reader_unlock(&conn->rco_call_rwlock);
		return;
	}

	rpc_connection_call_retain(call);
	g_mutex_lock(&call->rc_mtx);
	if (cancel_timeout_locked(call) != 0) {
		g_mutex_unlock(&call->rc_mtx);
		g_rw_lock_reader_unlock(&conn->rco_call_rwlock);
		rpc_connection_call_release(call);
		return;
	}

	g_rw_lock_reader_unlock(&conn->rco_call_rwlock);

	seqno = rpc_dictionary_get_int64(args, "seqno");
	payload = rpc_dictionary_get_value(args, "fragment");

	if (payload == NULL) {
		debugf("Fragment with no payload received on %p", conn);
		g_mutex_unlock(&call->rc_mtx);
		rpc_connection_call_release(call);
		return;
	}

	if (call->rc_callback) {
		item = g_malloc0(sizeof(*item));
		item->call = call;
		if (!rpc_run_callback(conn, item))
			g_free(item);
	}

	q_item = g_malloc(sizeof(*q_item));
	q_item->status = RPC_CALL_MORE_AVAILABLE;
	q_item->item = rpc_retain(payload);

	g_queue_push_tail(call->rc_queue, q_item);
	notify_signal(&call->rc_notify);
	g_mutex_unlock(&call->rc_mtx);
	rpc_connection_call_release(call);
}

static void
on_rpc_continue(rpc_connection_t conn, rpc_object_t args, rpc_object_t id)
{
	struct rpc_call *call;
	int64_t seqno = 0;
	int64_t increment = 1;

	rpc_object_unpack(args, "{i,i}",
	    "seqno", &seqno,
	    "increment", &increment);

	g_rw_lock_reader_lock(&conn->rco_icall_rwlock);
	call = g_hash_table_lookup(conn->rco_inbound_calls,
	    rpc_string_get_string_ptr(id));
	if (call == NULL) {
		g_rw_lock_reader_unlock(&conn->rco_icall_rwlock);
		if (conn->rco_error_handler != NULL)
			conn->rco_error_handler(RPC_SPURIOUS_RESPONSE, id);
		return;
	}

	rpc_connection_call_retain(call);
	g_mutex_lock(&call->rc_mtx);
	g_rw_lock_reader_unlock(&conn->rco_icall_rwlock);
	call->rc_consumer_seqno += increment;
	notify_signal(&call->rc_notify);
	g_mutex_unlock(&call->rc_mtx);
	rpc_connection_call_release(call);
}

static void
on_rpc_end(rpc_connection_t conn, rpc_object_t args __unused, rpc_object_t id)
{
	struct queue_item *q_item;
	rpc_call_t call;

	g_rw_lock_reader_lock(&conn->rco_call_rwlock);
	call = g_hash_table_lookup(conn->rco_calls,
	    rpc_string_get_string_ptr(id));
	if (call == NULL) {
		g_rw_lock_reader_unlock(&conn->rco_call_rwlock);
		if (conn->rco_error_handler != NULL)
			conn->rco_error_handler(RPC_SPURIOUS_RESPONSE, id);
		return;
	}

	rpc_connection_call_retain(call);
	g_mutex_lock(&call->rc_mtx);
	if (cancel_timeout_locked(call) != 0) {
		g_mutex_unlock(&call->rc_mtx);
		g_rw_lock_reader_unlock(&conn->rco_call_rwlock);
		rpc_connection_call_release(call);
		return;
	}

	g_rw_lock_reader_unlock(&conn->rco_call_rwlock);

	q_item = g_malloc(sizeof(*q_item));
	q_item->status = RPC_CALL_ENDED;
	q_item->item = rpc_retain(args);

	g_queue_push_tail(call->rc_queue, q_item);
	notify_signal(&call->rc_notify);
	g_mutex_unlock(&call->rc_mtx);
	rpc_connection_call_release(call);
}

static void
on_rpc_abort(rpc_connection_t conn, rpc_object_t args __unused, rpc_object_t id)
{
	struct rpc_call *call;

	g_rw_lock_reader_lock(&conn->rco_icall_rwlock);
	call = g_hash_table_lookup(conn->rco_inbound_calls,
	    rpc_string_get_string_ptr(id));
	if (call == NULL) {
		g_rw_lock_reader_unlock(&conn->rco_icall_rwlock);
		if (conn->rco_error_handler != NULL)
			conn->rco_error_handler(RPC_SPURIOUS_RESPONSE, id);
		return;
	}

	rpc_connection_call_retain(call);
	g_mutex_lock(&call->rc_mtx);
	g_rw_lock_reader_unlock(&conn->rco_icall_rwlock);
	call->rc_ended = true;
	call->rc_aborted = true;
	notify_signal(&call->rc_notify);
	if (call->rc_abort_handler) {
		/* call_abort_locked() will cause the call release */
		call_abort_locked(call);
	} else {
		g_mutex_unlock(&call->rc_mtx);
		rpc_connection_call_release(call);
	}
}

static void
on_rpc_error(rpc_connection_t conn, rpc_object_t args, rpc_object_t id)
{
	struct queue_item *q_item;
	rpc_call_t call;

	g_rw_lock_reader_lock(&conn->rco_call_rwlock);
	call = g_hash_table_lookup(conn->rco_calls,
	    rpc_string_get_string_ptr(id));
	if (call == NULL) {
		g_rw_lock_reader_unlock(&conn->rco_call_rwlock);

		// Support for older clients that do not support stream_start message
		if (rpc_error_get_code(args) == ENXIO) {
			g_rw_lock_reader_lock(&conn->rco_icall_rwlock);
			call = g_hash_table_lookup(conn->rco_inbound_calls,
			    rpc_string_get_string_ptr(id));
			if (call != NULL) {
				rpc_connection_call_retain(call);
				g_mutex_lock(&call->rc_mtx);
				g_rw_lock_reader_unlock(&conn->rco_icall_rwlock);
				call->rc_consumer_seqno++;
				notify_signal(&call->rc_notify);
				g_mutex_unlock(&call->rc_mtx);
				rpc_connection_call_release(call);
				return;
			}
			g_rw_lock_reader_unlock(&conn->rco_icall_rwlock);
		}

		if (conn->rco_error_handler != NULL)
			conn->rco_error_handler(RPC_SPURIOUS_RESPONSE, id);
		return;
	}

	rpc_connection_call_retain(call);
	g_mutex_lock(&call->rc_mtx);
	if (cancel_timeout_locked(call) != 0) {
		g_mutex_unlock(&call->rc_mtx);
		g_rw_lock_reader_unlock(&conn->rco_call_rwlock);
		rpc_connection_call_release(call);
		return;
	}

	g_rw_lock_reader_unlock(&conn->rco_call_rwlock);

	q_item = g_malloc(sizeof(*q_item));
	q_item->status = RPC_CALL_ERROR;
	q_item->item = rpc_retain(args);

	g_queue_push_tail(call->rc_queue, q_item);
	notify_signal(&call->rc_notify);
	g_mutex_unlock(&call->rc_mtx);
	rpc_connection_call_release(call);
}

static void
on_events_event(rpc_connection_t conn, rpc_object_t args,
    rpc_object_t id __unused)
{
	struct work_item *item;

	if (args == NULL)
		return;

	rpc_retain(args);
	item = g_malloc0(sizeof(*item));
	item->event = args;
	rpc_run_callback(conn, item);
}

static void
on_events_event_burst(rpc_connection_t conn, rpc_object_t args,
    rpc_object_t id __unused)
{

	rpc_array_apply(args, ^(size_t idx __unused, rpc_object_t value) {
		struct work_item *item;

		rpc_retain(value);
		item = g_malloc0(sizeof(*item));
		rpc_run_callback(conn, item);
		return ((bool)true);
	});
}

static void
on_events_subscribe(rpc_connection_t conn, rpc_object_t args,
    rpc_object_t id __unused)
{
	if (rpc_get_type(args) != RPC_TYPE_ARRAY)
		return;

	rpc_array_apply(args, ^(size_t index __unused, rpc_object_t value) {
		rpc_instance_t instance;
		struct rpc_subscription *sub;
		const char *path = NULL;
		const char *interface = NULL;
		const char *name = NULL;

		if (rpc_object_unpack(value, "{s,s,s}",
		    "name", &name,
		    "interface", &interface,
		    "path", &path) <1)
	    		return ((bool)true);

		instance = rpc_context_find_instance(
		    conn->rco_server->rs_context, path);

		if (instance == NULL)
			return ((bool)true);

		g_rw_lock_writer_lock(&conn->rco_subscription_rwlock);
		sub = rpc_connection_find_subscription(conn, path, interface, name);
		if (sub == NULL) {
			sub = g_malloc0(sizeof(*sub));
			sub->rsu_path = g_strdup(path);
			sub->rsu_interface = g_strdup(interface);
			sub->rsu_name = g_strdup(name);
			g_ptr_array_add(conn->rco_subscriptions, sub);
		}

		sub->rsu_refcount++;
		g_rw_lock_writer_unlock(&conn->rco_subscription_rwlock);
		return ((bool)true);
	});
}

static void 
rpc_subscription_release(struct rpc_subscription *sub)
{

	g_free(sub->rsu_path);
	g_free(sub->rsu_interface);
	g_free(sub->rsu_name);
	if (sub->rsu_handlers != NULL)
		g_ptr_array_free(sub->rsu_handlers, true);
	g_free(sub);
}

static void
rpc_rsh_release(struct rpc_subscription_handler *rsh)
{

	Block_release(rsh->rsh_handler);
	g_free(rsh);
}

static void
on_events_unsubscribe(rpc_connection_t conn, rpc_object_t args,
    rpc_object_t id __unused)
{
	if (rpc_get_type(args) != RPC_TYPE_ARRAY)
		return;

	rpc_array_apply(args, ^(size_t index __unused, rpc_object_t value) {
		struct rpc_subscription *sub;
		const char *path = NULL;
		const char *interface = NULL;
		const char *name = NULL;

		if (rpc_object_unpack(value, "{s,s,s}",
		    "name", &name,
		    "interface", &interface,
		    "path", &path) <1)
			return ((bool)true);

		g_rw_lock_writer_lock(&conn->rco_subscription_rwlock);
		sub = rpc_connection_find_subscription(conn, path, interface, name);
		if (sub == NULL)
			return ((bool)true);

		sub->rsu_refcount--;
		if (sub->rsu_refcount == 0)
			g_ptr_array_remove(conn->rco_subscriptions, sub);

		g_rw_lock_writer_unlock(&conn->rco_subscription_rwlock);
		return ((bool)true);
	});

}

static int
rpc_recv_msg(struct rpc_connection *conn, const void *frame, size_t len,
    int *fds, size_t nfds, struct rpc_credentials *creds)
{
	rpc_object_t msg = (rpc_object_t)frame;
	rpc_object_t msgt;
	int ret = 0;

	if (!rpc_connection_retain_if_open(conn)) {
		debugf("Rejecting msg, conn %p is closed", conn);
		return (-1);
	}

	debugf("received frame: addr=%p, len=%zu", frame, len);

	if (conn->rco_raw_handler != NULL) {
		ret = (conn->rco_raw_handler(frame, len, fds, nfds));
		goto done;
	}

	if ((conn->rco_flags & RPC_TRANSPORT_NO_SERIALIZE) == 0) {
		msg = rpc_msgpack_deserialize(frame, len);
		if (msg == NULL) {
			if (conn->rco_error_handler != NULL) {
				conn->rco_error_handler(RPC_SPURIOUS_RESPONSE,
				    NULL);
			}
			ret = -1;
			goto done;
		}
	} else
		rpc_retain(msg);

	if (rpc_get_type(msg) != RPC_TYPE_DICTIONARY) {
		if (conn->rco_error_handler != NULL)
			conn->rco_error_handler(RPC_SPURIOUS_RESPONSE, msg);
		rpc_release(msg);
		ret = -1;
		goto done;
	}

	msgt = rpct_deserialize(msg);
	rpc_release(msg);

	if (msgt == NULL) {
		if (conn->rco_error_handler != NULL)
			conn->rco_error_handler(RPC_SPURIOUS_RESPONSE, NULL);

		ret = -1;
		goto done;
	}

	if (creds != NULL)
		conn->rco_creds = *creds;

	rpc_restore_fds(msgt, fds, nfds);
	rpc_connection_dispatch(conn, msgt);

done:
	rpc_connection_release(conn);
	return (ret);
}

static void
call_abort_locked(struct rpc_call *call)
{
	rpc_abort_handler_t abt_fn;

	abt_fn = call->rc_abort_handler;
	call->rc_abort_handler = NULL;
	g_mutex_unlock(&call->rc_mtx);
	abt_fn();
	Block_release(abt_fn);
	rpc_connection_call_release(call);

}

static void
rpc_abort_worker(void *arg, void *data)
{
	struct rpc_call *call = arg;
	struct rpc_connection *conn = data;

	g_mutex_lock(&call->rc_mtx);
	g_assert(call->rc_conn == conn);
	if (call->rc_abort_handler)
		call_abort_locked(call);
	else {
		g_mutex_unlock(&call->rc_mtx);
		rpc_connection_call_release(call);
	}
}

static int
rpc_close(rpc_connection_t conn)
{
	GHashTableIter iter;
	struct rpc_call *call;
	struct queue_item *q_item;
	char *key;
	GError *err = NULL;

	g_mutex_lock(&conn->rco_mtx);
	if (conn->rco_aborted) {
		g_mutex_unlock(&conn->rco_mtx); /*another thread called first*/
		return (0);
	}

	conn->rco_aborted = true;
	if (conn->rco_server != NULL)
		conn->rco_closed = true;
	
	if (conn->rco_error_handler) {
		if (conn->rco_error != NULL) {
			conn->rco_error_handler(RPC_TRANSPORT_ERROR,
			    conn->rco_error);
		}
		conn->rco_error_handler(RPC_CONNECTION_CLOSED, NULL);
		Block_release(conn->rco_error_handler);
		conn->rco_error_handler = NULL;
	}

	rpc_connection_retain(conn);

	g_mutex_unlock(&conn->rco_mtx);

	/* Tear down all the running inbound/outbound calls */

	g_rw_lock_reader_lock(&conn->rco_icall_rwlock);
	g_hash_table_iter_init(&iter, conn->rco_inbound_calls);
	while (g_hash_table_iter_next(&iter, (gpointer)&key, (gpointer)&call)) {
		g_mutex_lock(&call->rc_mtx);
		call->rc_aborted = true;
		notify_signal(&call->rc_notify);

		if (call->rc_abort_handler) {
			rpc_connection_call_retain(call);
			g_mutex_unlock(&call->rc_mtx);
			g_thread_pool_push(conn->rco_callback_pool, call, &err);
			if (err != NULL) {
				g_error_free(err);
				Block_release(call->rc_abort_handler);
				call->rc_abort_handler = NULL;
				rpc_connection_call_release(call);
			}
		} else
			g_mutex_unlock(&call->rc_mtx);
	}
	g_rw_lock_reader_unlock(&conn->rco_icall_rwlock);

	g_rw_lock_reader_lock(&conn->rco_call_rwlock);
	g_hash_table_iter_init(&iter, conn->rco_calls);
	while (g_hash_table_iter_next(&iter, (gpointer)&key, (gpointer)&call)) {
		g_mutex_lock(&call->rc_mtx);
		/* Cancel timeout source */
		if (cancel_timeout_locked(call) != 0) {
			g_mutex_unlock(&call->rc_mtx);
			continue;
		}

		q_item = g_malloc(sizeof(*q_item));
		q_item->status = RPC_CALL_ERROR;
		q_item->item = rpc_error_create(ECONNABORTED,
		    "Connection closed", NULL);

		g_queue_push_tail(call->rc_queue, q_item);
		notify_signal(&call->rc_notify);
		g_mutex_unlock(&call->rc_mtx);
	}

	g_rw_lock_reader_unlock(&conn->rco_call_rwlock);

	if (conn->rco_closed)
		rpc_connection_do_close(conn, RPC_ABORTED);
	rpc_connection_release(conn);

	return (0);
}

static struct rpc_call *
rpc_call_alloc(rpc_connection_t conn, rpc_object_t id, const char *path,
    const char *interface, const char *method, rpc_object_t args)
{
	struct rpc_call *call;
	rpc_object_t call_args;

	if (method == NULL) {
		rpc_set_last_errorf(EINVAL,
		    "Method name not provided");
		return (NULL);
	}

	if (args != NULL) {
		if (rpc_get_type(args) != RPC_TYPE_ARRAY) {
			rpc_set_last_errorf(EINVAL,
			    "Method arguments must be an array");
			return (NULL);
		}
		call_args = rpc_retain(args);
	} else
		call_args = rpc_array_create();

	rpc_connection_retain(conn);
	call = g_malloc0(sizeof(*call));
	call->rc_refcount = 1;
	call->rc_queue = g_queue_new();
	call->rc_prefetch = 1;
	call->rc_conn = conn;
	call->rc_context = conn->rco_rpc_context;
	call->rc_path = g_strdup(path);
	call->rc_interface = g_strdup(interface);
	call->rc_method_name = g_strdup(method);
	call->rc_args = call_args;
	call->rc_id = id != NULL ? id : rpc_new_id();
	g_mutex_init(&call->rc_mtx);
	g_mutex_init(&call->rc_ref_mtx);
	notify_init(&call->rc_notify);

	return (call);
}

static int
rpc_send_frame(rpc_connection_t conn, rpc_object_t frame)
{
	void *buf = frame;
	int fds[MAX_FDS];
	rpc_object_t tmp;
	size_t len = 0, nfds = 0;
	int ret;

	if ((conn->rco_flags & RPC_TRANSPORT_NO_RPCT_SERIALIZE) == 0) {
		tmp = rpct_serialize(frame);
		rpc_release(frame);
		frame = tmp;
		buf = tmp;
	}

#ifdef RPC_TRACE
	rpc_trace("SEND", conn->rco_uri, frame);
#endif

	g_mutex_lock(&conn->rco_send_mtx);
	nfds = rpc_serialize_fds(frame, fds, NULL, 0);

	if ((conn->rco_flags & RPC_TRANSPORT_NO_SERIALIZE) == 0) {
		if (rpc_msgpack_serialize(frame, &buf, &len) != 0) {
			g_mutex_unlock(&conn->rco_send_mtx);
			rpc_release(frame);
			return (-1);
		}
	}

	ret = conn->rco_send_msg(conn->rco_arg, buf, len, fds, nfds);
	rpc_release(frame);

	if ((conn->rco_flags & RPC_TRANSPORT_NO_SERIALIZE) == 0)
		free(buf);

	g_mutex_unlock(&conn->rco_send_mtx);
	return (ret);
}

static struct rpc_subscription *
rpc_connection_find_subscription(rpc_connection_t conn, const char *path,
    const char *interface, const char *name)
{
	struct rpc_subscription *result;
	guint i;

	for (i = 0; i < conn->rco_subscriptions->len; i++) {
		result = g_ptr_array_index(conn->rco_subscriptions, i);

		if (g_strcmp0(result->rsu_path, path) != 0)
			continue;

		if (g_strcmp0(result->rsu_interface, interface) != 0)
			continue;

		if (g_strcmp0(result->rsu_name, name) != 0)
			continue;

		return (result);
	}

	return (NULL);
}

void
rpc_connection_send_err(rpc_connection_t conn, rpc_object_t id, int code,
    const char *descr, ...)
{
	rpc_object_t err;
	va_list ap;
	char *str;

	va_start(ap, descr);
	g_vasprintf(&str, descr, ap);
	va_end(ap);

	err = rpc_error_create(code, str, NULL);
	rpc_connection_send_errx(conn, id, err);
	g_free(str);
}

static int
rpc_call_wait_locked(rpc_call_t call)
{
	int ret = 0;

	while (g_queue_is_empty(call->rc_queue)) {
		g_mutex_unlock(&call->rc_mtx);
		ret = notify_wait(&call->rc_notify);
		if (ret < 0 && errno == EINTR) {
			rpc_set_last_errorf(EINTR, "Interrupted by signal");
			g_mutex_lock(&call->rc_mtx);
			return (-1);
		}

		g_mutex_lock(&call->rc_mtx);
	}

	return (ret);
}

static gboolean
rpc_call_timeout(gpointer user_data)
{
	struct queue_item *q_item;
	rpc_call_t call = user_data;

	if (g_source_is_destroyed(g_main_current_source()))
		return (false);

	g_mutex_lock(&call->rc_mtx);
	call->rc_timedout = true;

	/* make sure when we get the lock someone hasn't already handled this */
	if (call->rc_timeout == NULL ||
	    g_source_is_destroyed(call->rc_timeout)) {
		g_mutex_unlock(&call->rc_mtx);
		return false;
	}

	q_item = g_malloc(sizeof(*q_item));
	q_item->status = RPC_CALL_ERROR;
	q_item->item = rpc_error_create(ETIMEDOUT, "Call timed out", NULL);

	g_queue_push_tail(call->rc_queue, q_item);
	notify_signal(&call->rc_notify);
	g_mutex_unlock(&call->rc_mtx);

	return (false);
}

void
rpc_connection_send_errx(rpc_connection_t conn, rpc_object_t id __unused,
    rpc_object_t err)
{
	rpc_object_t frame;

	frame = rpc_pack_frame("rpc", "error", id, err);
	rpc_send_frame(conn, frame);
}

void
rpc_connection_send_response(rpc_connection_t conn, rpc_object_t id,
    rpc_object_t response)
{
	rpc_object_t frame;

	if (response == NULL)
		response = rpc_null_create();

	frame = rpc_pack_frame("rpc", "response", id, response);
	rpc_send_frame(conn, frame);
}

void
rpc_connection_send_start_stream(rpc_connection_t conn, rpc_object_t id,
    int64_t seqno)
{
	rpc_object_t frame;
	rpc_object_t args;

	args = rpc_dictionary_create();
	rpc_dictionary_set_int64(args, "seqno", seqno);
	frame = rpc_pack_frame("rpc", "start_stream", id, args);
	rpc_send_frame(conn, frame);
}

void
rpc_connection_send_fragment(rpc_connection_t conn, rpc_object_t id,
    int64_t seqno, rpc_object_t fragment)
{
	rpc_object_t frame;
	rpc_object_t args;

	args = rpc_dictionary_create();
	rpc_dictionary_set_int64(args, "seqno", seqno);
	rpc_dictionary_steal_value(args, "fragment", fragment);
	frame = rpc_pack_frame("rpc", "fragment", id, args);
	rpc_send_frame(conn, frame);
}

void
rpc_connection_send_end(rpc_connection_t conn, rpc_object_t id, int64_t seqno)
{
	rpc_object_t frame;
	rpc_object_t args;

	args = rpc_dictionary_create();
	rpc_dictionary_set_int64(args, "seqno", seqno);
	frame = rpc_pack_frame("rpc", "end", id, args);
	rpc_send_frame(conn, frame);
}

int
rpc_connection_call_retain(struct rpc_call *call)
{

	g_assert(call->rc_refcount > 0);
	g_mutex_lock(&call->rc_ref_mtx);
	if (call->rc_refcount < 1) {
		g_mutex_unlock(&call->rc_ref_mtx);
		return (-1);
	}

	++call->rc_refcount;

	g_mutex_unlock(&call->rc_ref_mtx);
	return (0);
}

int
rpc_connection_call_release(struct rpc_call *call)
{

	g_assert(call->rc_refcount > 0);
	g_mutex_lock(&call->rc_ref_mtx);

	if (call->rc_refcount > 1) {
		call->rc_refcount--;
		g_mutex_unlock(&call->rc_ref_mtx);
		return (0);
	} else if (call->rc_refcount < 1) {
		g_mutex_unlock(&call->rc_ref_mtx);
		return (-1);
	}

	call->rc_refcount = -1;
	g_mutex_unlock(&call->rc_ref_mtx);

	if (call->rc_callback != NULL)
		Block_release(call->rc_callback);

	if (call->rc_instance != NULL)
		rpc_instance_release(call->rc_instance);

	rpc_release(call->rc_err);
	rpc_release(call->rc_id);
	rpc_release(call->rc_args);
	g_free(call->rc_path);
	g_free(call->rc_interface);
	g_free(call->rc_method_name);
	notify_free(&call->rc_notify);
	g_mutex_clear(&call->rc_mtx);
	g_mutex_clear(&call->rc_ref_mtx);

	if (call->rc_queue != NULL)
		g_queue_free(call->rc_queue);

	rpc_connection_release(call->rc_conn); /*drop the call's ref */
	g_free(call);
	return (0);
}

void
rpc_connection_close_inbound_call(struct rpc_call *call)
{
	rpc_connection_t conn = call->rc_conn;

	rpc_connection_retain(conn);

	g_rw_lock_writer_lock(&conn->rco_icall_rwlock);

	if (!g_hash_table_remove(conn->rco_inbound_calls, rpc_string_get_string_ptr(
	    call->rc_id))) {
		g_rw_lock_writer_unlock(&conn->rco_icall_rwlock);
		rpc_connection_release(conn);
		return;
	}

	g_rw_lock_writer_unlock(&conn->rco_icall_rwlock);

	rpc_connection_call_release(call);
	rpc_connection_release(conn);
}

static rpc_object_t
rpc_new_id(void)
{
	char *str = rpc_generate_v4_uuid();
	rpc_object_t ret = rpc_string_create(str);

	g_free(str);
	return (ret);
}

static void
rpc_connection_set_default_fn_handlers(rpc_connection_t conn)
{

	conn->rco_fn_cbs.rcf_fn_respond = rpc_function_respond_impl;
	conn->rco_fn_cbs.rcf_fn_error = rpc_function_error_impl;
	conn->rco_fn_cbs.rcf_fn_error_ex = rpc_function_error_ex_impl;
	conn->rco_fn_cbs.rcf_fn_start_stream = rpc_function_start_stream_impl;
	conn->rco_fn_cbs.rcf_fn_yield = rpc_function_yield_impl;
	conn->rco_fn_cbs.rcf_fn_end = rpc_function_end_impl;
	conn->rco_fn_cbs.rcf_fn_kill = rpc_function_kill_impl;
	conn->rco_fn_cbs.rcf_should_abort = rpc_function_should_abort_impl;
	conn->rco_fn_cbs.rcf_set_async_abort_handler =
	    rpc_function_set_async_abort_handler_impl;
}

static rpc_connection_t
rpc_connection_init(void)
{
	struct rpc_connection *conn = g_malloc0(sizeof(*conn));

	g_mutex_init(&conn->rco_mtx);
	g_mutex_init(&conn->rco_ref_mtx);
	g_mutex_init(&conn->rco_send_mtx);
	g_rw_lock_init(&conn->rco_subscription_rwlock);
	g_rw_lock_init(&conn->rco_call_rwlock);
	g_rw_lock_init(&conn->rco_icall_rwlock);

	conn->rco_calls = g_hash_table_new(g_str_hash, g_str_equal);
	conn->rco_inbound_calls = g_hash_table_new(g_str_hash, g_str_equal);
	conn->rco_subscriptions = g_ptr_array_new_with_free_func((GDestroyNotify)rpc_subscription_release);
	conn->rco_rpc_timeout = DEFAULT_RPC_TIMEOUT;
	conn->rco_recv_msg = rpc_recv_msg;
	conn->rco_close = rpc_close;
	conn->rco_closed = false;
	conn->rco_aborted = false;
	conn->rco_refcnt = 1;
	conn->rco_arg = conn;
	rpc_connection_set_default_fn_handlers(conn);

	return(conn);
}

rpc_connection_t
rpc_connection_alloc(rpc_server_t server)
{
	struct rpc_connection *conn = NULL;
	GError *err = NULL;

	conn = rpc_connection_init();

	conn->rco_uri = server->rs_uri;
	conn->rco_flags = server->rs_flags;
	conn->rco_server = server;
	conn->rco_main_context = rpc_server_get_main_context(server);

	conn->rco_callback_pool = g_thread_pool_new(&rpc_abort_worker, conn,
	    g_get_num_processors(), false, &err);
	if (err != NULL) {
		g_free(err);
		rpc_connection_free_resources(conn);
		return NULL;
	}
	return (conn);
}

rpc_connection_t
rpc_connection_create(void *cookie, rpc_object_t params)
{
	GError *err = NULL;
	const struct rpc_transport *transport;
	struct rpc_connection *conn = NULL;
	struct rpc_client *client = cookie;
	char *scheme = NULL;

	scheme = g_uri_parse_scheme(client->rci_uri);
	transport = rpc_find_transport(scheme);
	g_free(scheme);

	if (transport == NULL) {
		rpc_set_last_error(ENXIO, "Transport not found", NULL);
		return (NULL);
	}

	conn = rpc_connection_init();

	conn->rco_client = client;
	conn->rco_flags = transport->flags;
	conn->rco_params = params;
	conn->rco_uri = client->rci_uri;
	conn->rco_main_context = rpc_client_get_main_context(client);

	conn->rco_callback_pool = g_thread_pool_new(&rpc_callback_worker, conn,
	    g_get_num_processors(), false, &err);
	rpc_connection_set_default_fn_handlers(conn);

	if (err != NULL) {
		g_free(err);
		goto fail;
	}
	if (transport->connect(conn, conn->rco_uri, params) != 0)
		goto fail;

	if (conn->rco_flags & RPC_TRANSPORT_FD_PASSING)
		conn->rco_supports_fd_passing = transport->is_fd_passing(conn);

	return (conn);
fail:
	if (conn != NULL)
		rpc_connection_free_resources(conn);
	return (NULL);
}

int
rpc_connection_register_context(rpc_connection_t conn, rpc_context_t ctx)
{

	g_mutex_lock(&conn->rco_mtx);
	if (conn->rco_closed) {
		g_mutex_unlock(&conn->rco_mtx);
		return (-1);
	}
	conn->rco_rpc_context = ctx;
	return (0);
}

int rpc_connection_get_subscription_count(rpc_connection_t conn)
{

	return (
	    rpc_connection_is_open(conn) ? conn->rco_subscriptions->len : -1);
}

static void
rpc_connection_free_resources(rpc_connection_t conn)
{

	g_assert_cmpint(g_hash_table_size(conn->rco_calls), ==, 0);
	g_assert_cmpint(g_hash_table_size(conn->rco_inbound_calls), ==, 0);
	g_hash_table_destroy(conn->rco_calls);
	g_hash_table_destroy(conn->rco_inbound_calls);

	if (conn->rco_subscriptions != NULL)
		g_ptr_array_free(conn->rco_subscriptions, true);

	if (conn->rco_callback_pool != NULL) {
		g_thread_pool_free(conn->rco_callback_pool, true, false);
		conn->rco_callback_pool = NULL;
	}

	rpc_release(conn->rco_error);
	g_free(conn->rco_endpoint_address);
	g_rw_lock_clear(&conn->rco_call_rwlock);
	g_rw_lock_clear(&conn->rco_icall_rwlock);
	g_rw_lock_clear(&conn->rco_subscription_rwlock);
}

int
rpc_connection_close(rpc_connection_t conn)
{

	conn->rco_closed = true;
	return (rpc_connection_do_close(conn, RPC_CLOSE_CALLED));
}

int
rpc_connection_do_close(rpc_connection_t conn, rpc_close_source_t source)
{
	rpc_abort_fn_t abort_func;

	debugf("%s aborted: %d conn: %p refcnt: %d  arg: %p, closed %d"
	    " released: %d, source: %d",
	    conn->rco_server ? "Server" : "Client",
	    conn->rco_aborted, conn, conn->rco_refcnt,
	    conn->rco_arg, conn->rco_closed, conn->rco_released, source);

	g_mutex_lock(&conn->rco_mtx);

	if ((conn->rco_abort != NULL) && !conn->rco_aborted) {
		abort_func = conn->rco_abort;
		conn->rco_abort = NULL;
		g_mutex_unlock(&conn->rco_mtx);

		rpc_connection_retain(conn);
		abort_func(conn->rco_arg);
		rpc_connection_release(conn);
		return (0);
	}
	if (conn->rco_released)
		goto done;

	if (conn->rco_client != NULL) {
		if (!conn->rco_closed)
			goto done;

		conn->rco_released = true;
		g_mutex_unlock(&conn->rco_mtx);

	} else if (conn->rco_server != NULL) {
		g_atomic_int_inc(&conn->rco_server->rs_conn_closed);
		conn->rco_released = true;
		g_mutex_unlock(&conn->rco_mtx);

		/* if server isn't closed this will undo server's ref */
		rpc_server_disconnect(conn->rco_server, conn);
	}
	/* undo connection's initial reference */
	rpc_connection_release(conn);
	return (0);
done:
	g_mutex_unlock(&conn->rco_mtx);
	return (0);
}

bool
rpc_connection_is_open(rpc_connection_t conn)
{

	return (!(conn->rco_closed || conn->rco_aborted || conn->rco_refcnt < 1));
}

int
rpc_connection_retain(rpc_connection_t conn)
{
	int ret;

	if (conn == NULL)
		return (-1);

	g_mutex_lock(&conn->rco_ref_mtx);
	g_assert(conn->rco_refcnt > 0);
	ret = ++conn->rco_refcnt;
	g_mutex_unlock(&conn->rco_ref_mtx);

	return (ret);
}

int
rpc_connection_release(rpc_connection_t conn)
{
	int ret;

	if (conn == NULL)
		return (-1);

	g_mutex_lock(&conn->rco_ref_mtx);
	g_assert(conn->rco_refcnt > 0);
	if (conn->rco_refcnt == 1) {
		g_assert(conn->rco_closed);

		if (conn->rco_release && conn->rco_arg) {
			conn->rco_release(conn->rco_arg);
			conn->rco_arg = NULL;
		}
		rpc_connection_free_resources(conn);

		debugf("%s in thread %p freed %p",
		    conn->rco_server ? "Server" : "Client",
		    g_thread_self(), conn);

		conn->rco_refcnt = -1;
		g_mutex_unlock(&conn->rco_ref_mtx);

		if (conn->rco_server != NULL) {
			/* undo connection's ref on server */
			rpc_server_release(conn->rco_server);
		}
		g_free(conn);
		return (0);
	}
	ret = --conn->rco_refcnt;
	g_mutex_unlock(&conn->rco_ref_mtx);

	return (ret);
}

int
rpc_connection_get_fd(rpc_connection_t conn)
{

	return (conn->rco_get_fd(conn->rco_arg));
}

void
rpc_connection_free(rpc_connection_t conn)
{


}

#ifdef ENABLE_LIBDISPATCH
int
rpc_connection_set_dispatch_queue(rpc_connection_t conn, dispatch_queue_t queue)
{
	GError *err = NULL;

	if (conn->rco_callback_pool == NULL)
		return (-1);

	g_thread_pool_set_max_threads(conn->rco_callback_pool, 0, &err);
	conn->rco_dispatch_queue = queue;
	return (0);
}
#endif

void
rpc_connection_dispatch(rpc_connection_t conn, rpc_object_t frame)
{
	rpc_object_t id;
	rpc_object_t args;
	const struct message_handler *h;
	const char *namespace;
	const char *name;

	/* Must be called with the connection retained */
	id = rpc_dictionary_get_value(frame, "id");
	namespace = rpc_dictionary_get_string(frame, "namespace");
	name = rpc_dictionary_get_string(frame, "name");

	if (id == NULL || namespace == NULL || name == NULL) {
		rpc_connection_send_err(conn, id, EINVAL, "Malformed request");
		return;
	}

	debugf("inbound call: namespace=%s, name=%s, id=%s", namespace, name,
	    rpc_string_get_string_ptr(id));

#ifdef RPC_TRACE
	rpc_trace("RECV", conn->rco_uri, frame);
#endif

	for (h = &handlers[0]; h->namespace != NULL; h++) {
		if (g_strcmp0(namespace, h->namespace))
			continue;

		if (g_strcmp0(name, h->name))
			continue;

		args = rpc_dictionary_get_value(frame, "args");
		h->handler(conn, args, id);
		rpc_release(frame);
		return;
	}

	rpc_connection_send_err(conn, id, ENXIO, "No request handler found");
	rpc_release(frame);
}

static struct rpc_subscription *
rpc_connection_subscribe_event_locked(rpc_connection_t conn, const char *path,
    const char *interface, const char *name)
{
	struct rpc_subscription *sub;
	rpc_object_t frame;
	rpc_object_t args;

	sub = rpc_connection_find_subscription(conn, path, interface, name);
	if (sub == NULL) {
		sub = g_malloc0(sizeof(*sub));
		sub->rsu_path = g_strdup(path);
		sub->rsu_interface = g_strdup(interface);
		sub->rsu_name = g_strdup(name);
		sub->rsu_handlers = g_ptr_array_new_with_free_func((GDestroyNotify)rpc_rsh_release);
		args = rpc_object_pack("[{s,s,s}]",
		    "path", path,
		    "interface", interface,
		    "name", name);

		frame = rpc_pack_frame("events", "subscribe", NULL, args);

		if (rpc_send_frame(conn, frame) != 0) {
			rpc_subscription_release(sub);
			return (NULL);
		}

		g_ptr_array_add(conn->rco_subscriptions, sub);
	}

	sub->rsu_refcount++;
	return (sub);
}

int
rpc_connection_subscribe_event(rpc_connection_t conn, const char *path,
    const char *interface, const char *name)
{
	struct rpc_subscription *sub;

	if (!rpc_connection_retain_if_open(conn)) {
		rpc_set_last_error(ECONNRESET, "Connection closed", NULL);
		return (-1);
	}

	g_rw_lock_writer_lock(&conn->rco_subscription_rwlock);
	sub = rpc_connection_subscribe_event_locked(conn, path, interface, name);
	g_rw_lock_writer_unlock(&conn->rco_subscription_rwlock);

	if (sub == NULL)
		rpc_set_last_error(EFAULT, "Unknown error", NULL);

	rpc_connection_release(conn);
	return (sub == NULL) ? -1 : 0;
}

int
rpc_connection_unsubscribe_event(rpc_connection_t conn, const char *path,
    const char *interface, const char *name)
{
	struct rpc_subscription *sub;
	int ret = 0;

	if (!rpc_connection_retain_if_open(conn)) {
		rpc_set_last_error(ECONNRESET, "Connection closed", NULL);
		return (-1);
	}

	g_rw_lock_writer_lock(&conn->rco_subscription_rwlock);
	sub = rpc_connection_find_subscription(conn, path, interface, name);
	if (sub == NULL) {
		g_rw_lock_writer_unlock(&conn->rco_subscription_rwlock);
		rpc_set_last_error(ENOENT, "Subscription not found", NULL);
		rpc_connection_release(conn);
		return (-1);
	}
	ret = rpc_connection_unsubscribe_event_locked(conn, sub);
	g_rw_lock_writer_unlock(&conn->rco_subscription_rwlock);
	rpc_connection_release(conn);
	return (ret);
}

static int
rpc_connection_unsubscribe_event_locked(rpc_connection_t conn,
    struct rpc_subscription *sub)
{
	rpc_object_t frame;
	rpc_object_t args;
	int ret = 0;

	/* Called with the connection retained and the subscription locked  */

	sub->rsu_refcount--;
	if (sub->rsu_refcount > 0)
		return (0);
 
	args = rpc_object_pack("[{s,s,s}]",
	    "path", sub->rsu_path,
	    "interface", sub->rsu_interface,
	    "name", sub->rsu_name);

	frame = rpc_pack_frame("events", "unsubscribe", NULL, args);
	ret = rpc_send_frame(conn, frame);

	g_ptr_array_remove(conn->rco_subscriptions, sub);

	return (ret);
}

static bool
rpc_connection_retain_if_open(rpc_connection_t conn)
{

	g_mutex_lock(&conn->rco_mtx);
	if (!rpc_connection_is_open(conn)) {
		g_mutex_unlock(&conn->rco_mtx);
		debugf("connection not open");
		rpc_set_last_error(ECONNRESET, "Connection closed", NULL);
		return (false);
	}
	rpc_connection_retain(conn);
	g_mutex_unlock(&conn->rco_mtx);
	return (true);
}

void *
rpc_connection_register_event_handler(rpc_connection_t conn, const char *path,
    const char *interface, const char *name, rpc_handler_t handler)
{
	struct rpc_subscription *sub;
	struct rpc_subscription_handler *rsh = NULL;

	if (!rpc_connection_retain_if_open(conn)) {
		rpc_set_last_error(ECONNRESET, "Connection closed", NULL);
		return (NULL);
	}

	g_rw_lock_writer_lock(&conn->rco_subscription_rwlock);

	sub = rpc_connection_subscribe_event_locked(conn, path, interface,
	    name);
	if (sub == NULL) {
		/* Couldn't inform server, error has been set */
		goto done;
	}
	if (sub->rsu_busy) {
		/* sub already existed, just undo the refcount ++ */
		sub->rsu_refcount--;
		rpc_set_last_error(EBUSY, "Subscription locked, retry", NULL);
		goto done;
	}
	rsh = g_malloc0(sizeof(*rsh));
	rsh->rsh_parent = sub;
	rsh->rsh_handler = Block_copy(handler);
	g_ptr_array_add(sub->rsu_handlers, rsh);
done:
	g_rw_lock_writer_unlock(&conn->rco_subscription_rwlock);
	rpc_connection_release(conn);

	return (rsh);
}

int
rpc_connection_unregister_event_handler(rpc_connection_t conn, void *cookie)
{
	struct rpc_subscription_handler *rsh = cookie;
	struct rpc_subscription *sub;
	int ret = 0;

	if (!rpc_connection_retain_if_open(conn)) {
		rpc_set_last_error(ECONNRESET, "Connection closed", NULL);
		return (-1);
	}

	sub = rsh->rsh_parent;

	g_rw_lock_writer_lock(&conn->rco_subscription_rwlock);
	if (sub->rsu_busy) {
		ret = -1;
		rpc_set_last_error(EBUSY, "Subscription locked, retry", NULL);
		goto done;
	}
	if (g_ptr_array_remove(sub->rsu_handlers, rsh))
		rpc_connection_unsubscribe_event_locked(conn, sub);

done:
	g_rw_lock_writer_unlock(&conn->rco_subscription_rwlock);
	rpc_connection_release(conn);
	return ret;
}

rpc_object_t
rpc_connection_call_syncv(rpc_connection_t conn, const char *path,
    const char *interface, const char *method, va_list ap)
{
	rpc_call_t call;
	rpc_object_t args;
	rpc_object_t result;
	rpc_object_t i;

	args = rpc_array_create();

	for (;;) {
		i = va_arg(ap, rpc_object_t);
		if (i == NULL)
			break;

		rpc_array_append_stolen_value(args, i);
	}

	if ((call = rpc_connection_call(conn, path, interface, method, args, NULL)) == NULL)
		return (NULL);

	rpc_call_wait(call);
	result = rpc_call_result_save(call);
	rpc_call_free(call);
	return (result);
}

rpc_object_t
rpc_connection_call_sync(rpc_connection_t conn, const char *path,
    const char *interface, const char *method, ...)
{
	rpc_object_t result;
	va_list ap;

	va_start(ap, method);
	result = rpc_connection_call_syncv(conn, path, interface, method, ap);
	va_end(ap);

	return (result);
}

rpc_object_t
rpc_connection_call_syncp(rpc_connection_t conn, const char *path,
    const char *interface, const char *method, const char *fmt, ...)
{
	rpc_object_t result;
	va_list ap;

	va_start(ap, fmt);
	result = rpc_connection_call_syncpv(conn, path, interface, method, fmt, ap);
	va_end(ap);

	return (result);
}

rpc_object_t rpc_connection_call_syncpv(rpc_connection_t conn,
    const char *path, const char *interface, const char *method,
    const char *fmt, va_list ap)
{
	rpc_call_t call;
	rpc_auto_object_t args = NULL;
	rpc_object_t result;

	args = rpc_object_vpack(fmt, ap);
	call = rpc_connection_call(conn, path, interface, method, args, NULL);

	if (call == NULL)
		return (NULL);

	rpc_call_wait(call);
	result = rpc_call_result_save(call);
	rpc_call_free(call);
	return (result);
}

rpc_object_t
rpc_connection_call_simple(rpc_connection_t conn, const char *name,
    const char *fmt, ...)
{
	va_list ap;
	rpc_object_t result;

	va_start(ap, fmt);
	result = rpc_connection_call_syncpv(conn, NULL, NULL, name, fmt, ap);
	va_end(ap);

	return (result);
}

rpc_call_t
rpc_connection_call(rpc_connection_t conn, const char *path,
    const char *interface, const char *name, rpc_object_t args,
    rpc_callback_t callback)
{
	struct rpc_call *call;
	rpc_object_t payload;
	rpc_object_t frame;

	if (!rpc_connection_retain_if_open(conn))
		return (NULL);

	call = rpc_call_alloc(conn, NULL, path, interface, name, args);
	if (call == NULL) {
		rpc_connection_release(conn);
		return (NULL);
	}

	call->rc_type = RPC_OUTBOUND_CALL;
	call->rc_callback = callback != NULL ? Block_copy(callback) : NULL;
	payload = rpc_dictionary_create();

	if (path != NULL)
		rpc_dictionary_set_string(payload, "path", path);

	if (interface != NULL)
		rpc_dictionary_set_string(payload, "interface", interface);

	rpc_dictionary_set_string(payload, "method", name);
	rpc_dictionary_set_value(payload, "args", call->rc_args);
	frame = rpc_pack_frame("rpc", "call", call->rc_id, payload);

	g_mutex_lock(&call->rc_mtx);
	g_rw_lock_writer_lock(&conn->rco_call_rwlock);
	g_hash_table_insert(conn->rco_calls,
	    (gpointer)rpc_string_get_string_ptr(call->rc_id), call);
	g_rw_lock_writer_unlock(&conn->rco_call_rwlock);

	call->rc_timeout = g_timeout_source_new_seconds(conn->rco_rpc_timeout);
	g_source_set_callback(call->rc_timeout, &rpc_call_timeout, call, NULL);
	g_mutex_lock(&conn->rco_mtx);
	g_source_attach(call->rc_timeout, conn->rco_main_context);
	g_mutex_unlock(&call->rc_mtx);
	g_mutex_unlock(&conn->rco_mtx);

	if (rpc_send_frame(conn, frame) != 0) {
		rpc_call_free(call);
		rpc_connection_release(conn);
		return (NULL);
	}

	rpc_connection_release(conn);
	return (call);
}

rpc_object_t
rpc_connection_get_property(rpc_connection_t conn, const char *path,
    const char *interface, const char *name)
{

	return (rpc_connection_call_syncp(conn, path, RPC_OBSERVABLE_INTERFACE,
	    "get", "[s,s]", interface, name));
}


rpc_object_t
rpc_connection_set_property(rpc_connection_t conn, const char *path,
    const char *interface, const char *name, rpc_object_t value)
{

	return (rpc_connection_call_syncp(conn, path, RPC_OBSERVABLE_INTERFACE,
	    "set", "[s,s,v]", interface, name, value));
}


rpc_object_t
rpc_connection_set_propertyp(rpc_connection_t conn, const char *path,
    const char *interface, const char *name, const char *fmt, ...)
{
	rpc_object_t value;
	va_list ap;

	va_start(ap, fmt);
	value = rpc_object_vpack(fmt, ap);
	va_end(ap);

	return (rpc_connection_set_property(conn, path, interface, name, value));
}


rpc_object_t
rpc_connection_set_propertypv(rpc_connection_t conn, const char *path,
    const char *interface, const char *name, const char *fmt, va_list ap)
{
	rpc_object_t value;

	value = rpc_object_vpack(fmt, ap);
	return (rpc_connection_set_property(conn, path, interface, name, value));
}

void *
rpc_connection_watch_property(rpc_connection_t conn, const char *path,
    const char *interface, const char *property,
    rpc_property_handler_t handler)
{
	/* add some memory leaks to we can remove them later */
	path = g_strdup(path);
	interface = g_strdup(interface);
	property = g_strdup(property);

	rpc_handler_t block = ^(const char *_p __unused, const char *_i __unused,
	    const char *_n __unused, rpc_object_t args) {
		const char *ev_iface;
		const char *ev_name;
		rpc_object_t ev_value;

		if (rpc_object_unpack(args, "{s,s,v}",
		    "interface", &ev_iface,
		    "name", &ev_name,
		    "value", &ev_value) < 3)
			return;

		if (g_strcmp0(interface, ev_iface) != 0)
			return;

		if (g_strcmp0(property, ev_name) != 0)
			return;

		handler(ev_value);
	};

	return (rpc_connection_register_event_handler(conn, path,
	    RPC_OBSERVABLE_INTERFACE, "changed", block));
}

int
rpc_connection_send_event(rpc_connection_t conn, const char *path,
    const char *interface, const char *name, rpc_object_t args)
{
	rpc_object_t frame;
	rpc_object_t event;
	struct rpc_subscription *sub;
	int ret = 0;

	if (!rpc_connection_retain_if_open(conn))
		return (-1);

	/* Any listeners? */
	g_rw_lock_reader_lock(&conn->rco_subscription_rwlock);
	if (rpc_connection_get_subscription_count(conn) < 1)
		goto done;

	sub = rpc_connection_find_subscription(conn, path, interface, name);
	if (sub == NULL)
		goto done;

	event = rpc_object_pack("{s,s,s,v}",
	    "path", path,
	    "interface", interface,
	    "name", name,
	    "args", rpc_retain(args));

	frame = rpc_pack_frame("events", "event", NULL, event);
	ret = rpc_send_frame(conn, frame);

done:
	g_rw_lock_reader_unlock(&conn->rco_subscription_rwlock);
	rpc_connection_release(conn);
	return (ret);
}

int
rpc_connection_send_raw_message(rpc_connection_t conn, const void *msg,
    size_t len, const int *fds, size_t nfds)
{
	int ret;

	g_mutex_lock(&conn->rco_mtx);
	ret = conn->rco_send_msg(conn->rco_arg, msg, len, fds, nfds);
	g_mutex_unlock(&conn->rco_mtx);

	return (ret);
}

void
rpc_connection_set_raw_message_handler(rpc_connection_t conn,
    rpc_raw_handler_t handler)
{

	g_mutex_lock(&conn->rco_mtx);
	if (conn->rco_raw_handler != NULL) {
		Block_release(conn->rco_raw_handler);
		conn->rco_raw_handler = NULL;
	}

	if (handler == NULL) {
		g_mutex_unlock(&conn->rco_mtx);
		return;
	}

	conn->rco_raw_handler = Block_copy(handler);
	g_mutex_unlock(&conn->rco_mtx);
}

void
rpc_connection_set_event_handler(rpc_connection_t conn, rpc_handler_t h)
{

	Block_release(conn->rco_event_handler);
	conn->rco_event_handler = Block_copy(h);
}

void
rpc_connection_set_error_handler(rpc_connection_t conn, rpc_error_handler_t h)
{

	Block_release(conn->rco_error_handler);
	conn->rco_error_handler = Block_copy(h);
}

const char *
rpc_connection_get_remote_address(rpc_connection_t conn)
{

	return (conn->rco_endpoint_address);
}


bool
rpc_connection_supports_fd_passing(rpc_connection_t conn)
{

	return (conn->rco_supports_fd_passing);
}

bool
rpc_connection_supports_credentials(rpc_connection_t conn)
{

	return ((bool)(conn->rco_flags & RPC_TRANSPORT_CREDENTIALS));
}

bool
rpc_connection_has_credentials(rpc_connection_t conn)
{

	return (conn->rco_has_creds);
}


uid_t
rpc_connection_get_remote_uid(rpc_connection_t conn)
{

	return (conn->rco_creds.rcc_uid);
}


gid_t
rpc_connection_get_remote_gid(rpc_connection_t conn)
{

	return (conn->rco_creds.rcc_gid);
}

pid_t
rpc_connection_get_remote_pid(rpc_connection_t conn)
{

	return (conn->rco_creds.rcc_pid);
}

int
rpc_call_wait(rpc_call_t call)
{
	int ret;

	g_assert_nonnull(call);
	g_mutex_lock(&call->rc_mtx);
	ret = rpc_call_wait_locked(call);
	g_mutex_unlock(&call->rc_mtx);

	return (ret);
}

int
rpc_call_continue(rpc_call_t call, bool sync)
{
	struct queue_item *q_item;
	rpc_call_status_t status;
	rpc_object_t frame;
	int64_t seqno;
	int ret = 0;

	g_mutex_lock(&call->rc_mtx);
	status = rpc_call_status_locked(call);

	if (status != RPC_CALL_IN_PROGRESS &&
	    status != RPC_CALL_MORE_AVAILABLE &&
	    status != RPC_CALL_STREAM_START) {
		rpc_set_last_errorf(ENXIO, "Not an open streaming call");
		g_mutex_unlock(&call->rc_mtx);
		return (-1);
	}

	if (call->rc_consumer_seqno == call->rc_producer_seqno) {
		seqno = call->rc_producer_seqno + 1;
		frame = rpc_pack_frame("rpc", "continue", call->rc_id, rpc_object_pack(
		    "{i,i}",
		    "seqno", seqno,
		    "increment", call->rc_prefetch));

		if (rpc_send_frame(call->rc_conn, frame) != 0) {
			q_item = g_malloc0(sizeof(*q_item));
			q_item->status = RPC_CALL_ERROR;
			q_item->item = rpc_retain(rpc_get_last_error());
			g_queue_push_tail(call->rc_queue, q_item);
			ret = -1;
		}

		call->rc_producer_seqno += call->rc_prefetch;
	}

	call->rc_consumer_seqno++;

	/* It is assumed that the caller retains q_item->item if it is needed */
	q_item = g_queue_pop_head(call->rc_queue);
	rpc_release(q_item->item);
	g_free(q_item);

	if (sync && ret == 0) {
		if (rpc_call_wait_locked(call) < 0) {
			g_mutex_unlock(&call->rc_mtx);
			return (-1);
		}

		g_mutex_unlock(&call->rc_mtx);
		return (rpc_call_success(call));
	}

	g_mutex_unlock(&call->rc_mtx);
	return (ret);
}

int
rpc_call_abort(rpc_call_t call)
{
	struct queue_item *q_item;
	rpc_call_status_t status;
	rpc_object_t frame;

	g_mutex_lock(&call->rc_mtx);
	status = rpc_call_status_locked(call);

	if (status != RPC_CALL_IN_PROGRESS &&
	    status != RPC_CALL_MORE_AVAILABLE &&
	    status != RPC_CALL_STREAM_START) {
		errno = EINVAL;
		g_mutex_unlock(&call->rc_mtx);
		return (-1);
	}

	frame = rpc_pack_frame("rpc", "abort", call->rc_id, rpc_null_create());
	if (rpc_send_frame(call->rc_conn, frame) != 0) {
		g_mutex_unlock(&call->rc_mtx);
		return (-1);
	}

	if (cancel_timeout_locked(call) == 0) {
		q_item = g_malloc(sizeof(*q_item));
		q_item->status = RPC_CALL_ABORTED;
		q_item->item = NULL;
		g_queue_push_tail(call->rc_queue, q_item);
	}

	g_mutex_unlock(&call->rc_mtx);
	return (0);
}

int
rpc_call_set_prefetch(_Nonnull rpc_call_t call, size_t nitems)
{

	g_mutex_lock(&call->rc_mtx);
	call->rc_prefetch = (int64_t)nitems;
	g_mutex_unlock(&call->rc_mtx);
	return (0);
}

inline int
rpc_call_timedwait(rpc_call_t call, const struct timespec *ts)
{
	int ret = 0;

	g_mutex_lock(&call->rc_mtx);
	while (g_queue_is_empty(call->rc_queue)) {
		g_mutex_unlock(&call->rc_mtx);
		if (!notify_timedwait(&call->rc_notify, ts)) {
			ret = -1;
			break;
		}
		g_mutex_lock(&call->rc_mtx);
	}

	g_mutex_unlock(&call->rc_mtx);
	return (ret);
}

int
rpc_call_success(rpc_call_t call)
{

	return (rpc_call_status(call) == RPC_CALL_DONE);
}

static inline rpc_call_status_t
rpc_call_status_locked(rpc_call_t call)
{
	struct queue_item *q_item;

	if (!g_queue_is_empty(call->rc_queue)) {
		q_item = g_queue_peek_head(call->rc_queue);
		return (q_item->status);
	}

	return (RPC_CALL_IN_PROGRESS);
}

rpc_call_status_t
rpc_call_status(rpc_call_t call)
{
	rpc_call_status_t result;

	g_mutex_lock(&call->rc_mtx);
	result = rpc_call_status_locked(call);
	g_mutex_unlock(&call->rc_mtx);
	return (result);
}

inline rpc_object_t
rpc_call_result(rpc_call_t call)
{
	struct queue_item *q_item;

	g_mutex_lock(&call->rc_mtx);
	q_item = g_queue_peek_head(call->rc_queue);
	g_mutex_unlock(&call->rc_mtx);

	return (q_item != NULL ? q_item->item : NULL);
}

static inline rpc_object_t
rpc_call_result_save(rpc_call_t call)
{
	struct queue_item *q_item;

	g_mutex_lock(&call->rc_mtx);
	q_item = g_queue_peek_head(call->rc_queue);
	if (q_item != NULL)
		rpc_retain(q_item->item);
	g_mutex_unlock(&call->rc_mtx);

	return (q_item != NULL ? q_item->item : NULL);
}

void
rpc_call_free(rpc_call_t call)
{
	rpc_connection_t conn = call->rc_conn;
	struct queue_item *q_item;

	rpc_connection_retain(conn);

	g_mutex_lock(&call->rc_mtx);
	cancel_timeout_locked(call);
	while (!g_queue_is_empty(call->rc_queue)) {
		q_item = g_queue_pop_head(call->rc_queue);
		rpc_release(q_item->item);
		g_free(q_item);
	}
	g_mutex_unlock(&call->rc_mtx);

	g_rw_lock_writer_lock(&conn->rco_call_rwlock);
	g_hash_table_remove(conn->rco_calls,
	    (gpointer)rpc_string_get_string_ptr(call->rc_id));
	g_rw_lock_writer_unlock(&conn->rco_call_rwlock);

	rpc_connection_call_release(call);
	rpc_connection_release(conn);
}
