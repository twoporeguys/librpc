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

#ifndef LIBRPC_CONNECTION_H
#define LIBRPC_CONNECTION_H

#include <Block.h>
#include <sys/time.h>
#include <rpc/object.h>

/**
 * @file connection.h
 *
 * RPC connection API.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct rpc_connection;
struct rpc_call;

typedef enum rpc_error_code
{
	INVALID_JSON_RESPONSE = 1,
	CONNECTION_TIMEOUT,
	CONNECTION_CLOSED,
	RPC_CALL_TIMEOUT,
	SPURIOUS_RPC_RESPONSE,
	LOGOUT,
	OTHER
} error_code_t;

/**
 * Enumerates possible remote procedure call status values.
 */
typedef enum rpc_call_status
{
	RPC_CALL_IN_PROGRESS,		/**< Call in progress */
	RPC_CALL_MORE_AVAILABLE,	/**< Streaming response, more data available */
	RPC_CALL_DONE,				/**< Call finished, response received */
	RPC_CALL_ERROR				/**< Call finished, error received */
} rpc_call_status_t;

typedef struct rpc_connection *rpc_connection_t;
typedef struct rpc_call *rpc_call_t;
typedef void (^rpc_handler_t)(const char *name, rpc_object_t args);
typedef bool (^rpc_callback_t)(rpc_object_t args, rpc_call_status_t status);

/**
 * Converts function pointer to a ::rpc_handler_t block type.
 */
#define	RPC_HANDLER(_fn, _arg) 						\
	^(const char *_name, rpc_object_t _args) {			\
		_fn(_arg, _name, _args);				\
	}

/**
 * Converts function pointer to a ::rpc_callback_t block type.
 */
#define	RPC_CALLBACK(_fn, _arg) 					\
	^(rpc_object_t _args, rpc_call_status_t _status) {		\
		return ((bool)_fn(_arg, _args, _status));		\
	}

rpc_connection_t rpc_connection_create(const char *uri, int flags);
int rpc_connection_close(rpc_connection_t conn);
int rpc_connection_subscribe_event(rpc_connection_t conn, const char *name);
int rpc_connection_unsubscribe_event(rpc_connection_t conn, const char *name);
void *rpc_connection_register_event_handler(rpc_connection_t conn,
    const char *name, rpc_handler_t handler);
void rpc_connection_unregister_event_handler(rpc_connection_t conn,
    const char *name, void *cookie);
rpc_object_t rpc_connection_call_sync(rpc_connection_t conn,
    const char *method, ...);
rpc_object_t rpc_connection_call_syncv(rpc_connection_t conn,
    const char *method, va_list ap);
rpc_call_t rpc_connection_call(rpc_connection_t conn, const char *name,
    rpc_object_t args, rpc_callback_t callback);
int rpc_connection_send_event(rpc_connection_t conn, const char *name,
    rpc_object_t args);
void rpc_connection_set_event_handler(rpc_connection_t conn,
    rpc_handler_t handler);

/**
 * Waits for a call to change status.
 *
 * @param call Call to wait on
 * @return 0 on success, -1 on failure
 */
int rpc_call_wait(rpc_call_t call);

/**
 * Requiests a next chunk from a call.
 *
 * @param call
 * @param sync
 * @return
 */
int rpc_call_continue(rpc_call_t call, bool sync);

/**
 * Aborts a pending call.
 * @param call
 * @return
 */
int rpc_call_abort(rpc_call_t call);
int rpc_call_timedwait(rpc_call_t call, const struct timespec *ts);
int rpc_call_success(rpc_call_t call);
int rpc_call_status(rpc_call_t call);

/**
 * Returns a call result (or a current fragment).
 *
 * @param call Call to get result from
 * @return Result
 */
rpc_object_t rpc_call_result(rpc_call_t call);

/**
 * Frees a rpc_call_t object.
 *
 * @param call Call to free
 */
void rpc_call_free(rpc_call_t call);

#ifdef __cplusplus
}
#endif

#endif //LIBRPC_CONNECTION_H
