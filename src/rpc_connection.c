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
#include "internal.h"

#ifdef __FreeBSD__
#include <uuid.h>
#endif

#if defined(__APPLE__) or defined(__linux__)
#include <uuid/uuid.h>
#endif

static rpc_object_t
rpc_new_id(void)
{
	char *str;
	uuid_t uuid;
	uint32_t status;
	rpc_object_t ret;

	uuid_create(&uuid, &status);
	if (status != uuid_s_ok)
		return (NULL);

	uuid_to_string(&uuid, &str, &status);
	if (status != uuid_s_ok)
		return (NULL);

	ret = rpc_string_create(str);
	free(str);
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
rpc_connection_send_event(rpc_connection_t const char *name, rpc_object_t args)
{

}

void
rpc_connection_set_event_handler(rpc_connection_t conn, rpc_handler_t handler)
{

}

int
rpc_call_wait(rpc_call_t call)
{

}

int
rpc_call_continue(rpc_call_t call, bool sync)
{

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