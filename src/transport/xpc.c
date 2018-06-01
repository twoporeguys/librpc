/*
 * Copyright 2018 Two Pore Guys, Inc.
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
 */

#include <errno.h>
#include <glib.h>
#include <libsoup/soup.h>
#include <dispatch/dispatch.h>
#include <xpc/xpc.h>
#include "../linker_set.h"
#include "../internal.h"

static int xpc_connect(struct rpc_connection *, const char *, rpc_object_t);
static int xpc_listen(struct rpc_server *, const char *, rpc_object_t);
static xpc_object_t xpc_from_rpc(rpc_object_t);
static rpc_object_t xpc_to_rpc(xpc_object_t);
static int xpc_abort(void *);
static void xpc_conn_release(void *);

struct xpc_server
{
	xpc_connection_t 	xpc_handle;
	dispatch_queue_t 	queue;
};

struct xpc_connection
{
	xpc_connection_t	xpc_handle;
	dispatch_queue_t 	queue;
	struct rpc_connection *	conn;
};

static const struct rpc_transport xpc_transport = {
	.name = "xpc",
	.schemas = {"xpc", "xpc+mach", NULL},
	.connect = xpc_connect,
	.listen = xpc_listen,
	.flags = RPC_TRANSPORT_NO_SERIALIZE
};

static xpc_object_t
xpc_from_rpc(rpc_object_t obj)
{
	xpc_object_t ret;
	rpc_object_t extra;
	rpc_object_t stack;

	if (obj == NULL)
		return (NULL);

	switch (rpc_get_type(obj)) {
	case RPC_TYPE_NULL:
		return (xpc_null_create());

	case RPC_TYPE_INT64:
		return (xpc_int64_create(rpc_int64_get_value(obj)));

	case RPC_TYPE_UINT64:
		return (xpc_uint64_create(rpc_uint64_get_value(obj)));

	case RPC_TYPE_DOUBLE:
		return (xpc_double_create(rpc_double_get_value(obj)));

	case RPC_TYPE_BOOL:
		return (xpc_bool_create(rpc_bool_get_value(obj)));

	case RPC_TYPE_DATE:
		return (xpc_date_create(rpc_date_get_value(obj)));

	case RPC_TYPE_STRING:
		return (xpc_string_create(rpc_string_get_string_ptr(obj)));

	case RPC_TYPE_BINARY:
		return (xpc_data_create(
		    rpc_data_get_bytes_ptr(obj),
		    rpc_data_get_length(obj)));

	case RPC_TYPE_ARRAY:
		ret = xpc_array_create(NULL, 0);
		rpc_array_apply(obj, ^(size_t idx, rpc_object_t value) {
			xpc_object_t item = xpc_from_rpc(value);
			xpc_array_append_value(ret, item);
			xpc_release(item);
			return ((bool)true);
		});

		return (ret);

	case RPC_TYPE_DICTIONARY:
		ret = xpc_dictionary_create(NULL, NULL, 0);
		rpc_dictionary_apply(obj, ^(const char *key, rpc_object_t value) {
			xpc_object_t item = xpc_from_rpc(value);
			xpc_dictionary_set_value(ret, key, item);
			xpc_release(item);
			return ((bool)true);
		});

		return (ret);

	case RPC_TYPE_ERROR:
		extra = rpc_error_get_extra(obj);
		stack = rpc_error_get_stack(obj);
		ret = xpc_dictionary_create(NULL, NULL, 0);
		xpc_dictionary_set_string(ret, "$type", "error");
		xpc_dictionary_set_int64(ret, "code", rpc_error_get_code(obj));
		xpc_dictionary_set_string(ret, "message", rpc_error_get_message(obj));
		xpc_dictionary_set_value(ret, "extra", extra != NULL
		    ? xpc_from_rpc(extra)
		    : xpc_null_create());

		xpc_dictionary_set_value(ret, "stack", stack != NULL
		    ? xpc_from_rpc(stack)
		    : xpc_null_create());

		return (ret);

	default:
		break;
	}

	g_assert_not_reached();
}

static rpc_object_t
xpc_to_rpc(xpc_object_t obj)
{
	rpc_object_t ret;
	xpc_type_t type = xpc_get_type(obj);
	const char *dtype;
	void *blob;

	if (obj == NULL)
		return (NULL);

	if (type == XPC_TYPE_NULL)
		return (rpc_null_create());

	if (type == XPC_TYPE_INT64)
		return (rpc_int64_create(xpc_int64_get_value(obj)));

	if (type == XPC_TYPE_UINT64)
		return (rpc_uint64_create(xpc_uint64_get_value(obj)));

	if (type == XPC_TYPE_DOUBLE)
		return (rpc_double_create(xpc_double_get_value(obj)));

	if (type == XPC_TYPE_BOOL)
		return (rpc_bool_create(xpc_bool_get_value(obj)));

	if (type == XPC_TYPE_DATE)
		return (rpc_date_create(xpc_date_get_value(obj)));

	if (type == XPC_TYPE_STRING)
		return (rpc_string_create(xpc_string_get_string_ptr(obj)));

	if (type == XPC_TYPE_DATA) {
		blob = g_memdup(xpc_data_get_bytes_ptr(obj),
		    (guint)xpc_data_get_length(obj));

		return (rpc_data_create(blob, xpc_data_get_length(obj),
		    ^(void *ptr) { g_free(ptr); }));
	}

	if (type == XPC_TYPE_ARRAY) {
		ret = rpc_array_create();
		xpc_array_apply(obj, ^(size_t idx, xpc_object_t value) {
			rpc_array_append_stolen_value(ret, xpc_to_rpc(value));
			return ((bool)true);
		});

		return (ret);
	}

	if (type == XPC_TYPE_DICTIONARY) {
		dtype = xpc_dictionary_get_string(obj, "$type");

		if (g_strcmp0(dtype, "error") == 0) {
			return (rpc_error_create_with_stack(
			    (int)xpc_dictionary_get_int64(obj, "code"),
			    xpc_dictionary_get_string(obj, "message"),
			    xpc_to_rpc(xpc_dictionary_get_value(obj, "extra")),
			    xpc_to_rpc(xpc_dictionary_get_value(obj, "stack"))));
		}

		ret = rpc_dictionary_create();
		xpc_dictionary_apply(obj, ^(const char *key, xpc_object_t value) {
			rpc_dictionary_steal_value(ret, key,  xpc_to_rpc(value));
			return ((bool)true);
		});

		return (ret);
	}

	g_assert_not_reached();
}

static int
xpc_send_msg(void *arg, void *buf, size_t size __unused,
    const int *fds __unused, size_t nfds __unused)
{
	struct xpc_connection *conn = arg;
	rpc_object_t obj = buf;
	xpc_object_t msg;

	msg = xpc_from_rpc(obj);
	xpc_connection_send_message(conn->xpc_handle, msg);
	xpc_release(msg);
	return (0);
}

static int
xpc_abort(void *arg)
{
	struct xpc_connection *conn = arg;

	xpc_connection_cancel(conn->xpc_handle);
	return (0);
}

static void
xpc_conn_release(void *arg)
{

}

static int
xpc_connect(struct rpc_connection *conn, const char *uri_string,
    rpc_object_t params __unused)
{
	SoupURI *uri;
	struct xpc_connection *xconn;

	uri = soup_uri_new(uri_string);
	if (uri == NULL) {
		rpc_set_last_errorf(EINVAL, "Invalid URI");
		return (-1);
	}

	xconn = g_malloc0(sizeof(*xconn));
	xconn->queue = dispatch_queue_create("xpc client", DISPATCH_QUEUE_SERIAL);
	xconn->xpc_handle = g_strcmp0(uri->scheme, "xpc") == 0
	    ? xpc_connection_create(uri->host, xconn->queue)
	    : xpc_connection_create_mach_service(uri->host, xconn->queue, 0);

	conn->rco_send_msg = xpc_send_msg;
	conn->rco_abort = xpc_abort;
	conn->rco_release = xpc_conn_release;
	conn->rco_arg = xconn;

	xpc_connection_set_event_handler(xconn->xpc_handle, ^(xpc_object_t msg) {
		if (xpc_get_type(msg) == XPC_TYPE_ERROR) {
			conn->rco_close(conn);
			return;
		}

		rpc_object_t obj = xpc_to_rpc(msg);
		conn->rco_recv_msg(conn, obj, 0, NULL, 0, NULL);
	});

	xpc_connection_resume(xconn->xpc_handle);
	return (0);
}

static int
xpc_listen(struct rpc_server *conn, const char *uri_string,
    rpc_object_t params __unused)
{
	SoupURI *uri;
	struct xpc_server *xserver;

	uri = soup_uri_new(uri_string);
	if (uri == NULL) {
		conn->rs_error = rpc_error_create(EINVAL, "Invalid URI", NULL);
		return (-1);
	}

	xserver = g_malloc0(sizeof(*xserver));
	xserver->queue = dispatch_queue_create("xpc server", DISPATCH_QUEUE_CONCURRENT);
	xserver->xpc_handle = xpc_connection_create_mach_service(uri->host,
	    NULL, XPC_CONNECTION_MACH_SERVICE_LISTENER);

	xpc_connection_set_event_handler(xserver->xpc_handle, ^(xpc_object_t peer) {
		struct xpc_connection *xconn;

		xconn = g_malloc0(sizeof(*xconn));
		xconn->xpc_handle = peer;
		xconn->conn = rpc_connection_alloc(conn);
		xconn->conn->rco_send_msg = xpc_send_msg;
		xconn->conn->rco_abort = xpc_abort;
		xconn->conn->rco_release = xpc_conn_release;
		xconn->conn->rco_arg = xconn;

		xpc_connection_set_event_handler(peer, ^(xpc_object_t msg) {
			if (xpc_get_type(msg) == XPC_TYPE_ERROR) {
				xconn->conn->rco_close(xconn->conn);
				return;
			}

			rpc_object_t obj = xpc_to_rpc(msg);
			xconn->conn->rco_recv_msg(xconn->conn, obj, 0, NULL, 0, NULL);
		});

		conn->rs_accept(conn, xconn->conn);
		xpc_connection_resume(peer);
	});

	xpc_connection_resume(xserver->xpc_handle);
	return (0);
}

DECLARE_TRANSPORT(xpc_transport);
