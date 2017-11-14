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

/**
 * Enumerates possible RPC error codes.
 */
typedef enum rpc_error_code
{
	RPC_INVALID_RESPONSE = 1,	/**< Received unreadable response */
	RPC_CONNECTION_TIMEOUT,		/**< Connection timed out */
	RPC_CONNECTION_CLOSED,		/**< Disconnect received */
	RPC_CALL_TIMEOUT,		/**< Request timed out */
	RPC_SPURIOUS_RESPONSE,		/**< Response to unknown request */
	RPC_LOGOUT,			/**< Logged out by server */
	RPC_OTHER			/**< Other or unknown reason */
} rpc_error_code_t;

/**
 * Enumerates possible remote procedure call status values.
 */
typedef enum rpc_call_status
{
	RPC_CALL_IN_PROGRESS,		/**< Call in progress */
	RPC_CALL_MORE_AVAILABLE,	/**< Streaming response, more data available */
	RPC_CALL_DONE,			/**< Call finished, response received */
	RPC_CALL_ERROR,			/**< Call finished, error received */
	RPC_CALL_ABORTED		/**< Call was aborted by the user */
} rpc_call_status_t;

/**
 * Definition of RPC connection pointer.
 */
typedef struct rpc_connection *rpc_connection_t;

/**
 * Definition of RPC call pointer.
 */
typedef struct rpc_call *rpc_call_t;

/**
 * Definition of RPC event handler block type.
 */
typedef void (^rpc_handler_t)(const char *name, rpc_object_t args);

/**
 * Definition of RPC error handler block type.
 */
typedef void (^rpc_error_handler_t)(rpc_error_code_t code, rpc_object_t args);

/**
 * Definition of RPC callback block type.
 */
typedef bool (^rpc_callback_t)(rpc_object_t args, rpc_call_status_t status);

/**
 * Converts function pointer to a ::rpc_handler_t block type.
 */
#define	RPC_HANDLER(_fn, _arg) 						\
	^(const char *_name, rpc_object_t _args) {			\
		_fn(_arg, _name, _args);				\
	}

/**
 * Converts function pointer to a ::rpc_error_handler_t block type.
 */
#define	RPC_ERROR_HANDLER(_fn, _arg) 					\
	^(rpc_error_code_t _code, rpc_object_t _args) {			\
		_fn(_arg, _code, _args);				\
	}

/**
 * Converts function pointer to a ::rpc_callback_t block type.
 */
#define	RPC_CALLBACK(_fn, _arg) 					\
	^(rpc_object_t _args, rpc_call_status_t _status) {		\
		return ((bool)_fn(_arg, _args, _status));		\
	}

rpc_connection_t rpc_connection_create(void *cookie, rpc_object_t params);

/**
 * Closes the connection and frees all resources associated with it.
 *
 * @param conn Connection to close
 * @return 0 on success, -1 on failure
 */
int rpc_connection_close(rpc_connection_t conn);

/**
 * Subscribes for an event.
 *
 * This function can be called multiple times on a single event name -
 * subsequent calls will not send a subscribe message to the server,
 * but instead increment internal reference count for a subscription.
 *
 * Calls to rpc_connection_subscribe_event() must be paired with
 * rpc_connection_unsubscribe_event().
 *
 * @param conn Connection to subscribe on
 * @param name Event name
 * @return 0 on success, -1 on failure
 */
int rpc_connection_subscribe_event(rpc_connection_t conn, const char *name);

/**
 * Undoes previous event subscription.
 *
 * This function may either:
 * - decrement reference count of subscription for given event
 * - send unsubscribe message to the server (when subscription reference count
 *   reached value of 0)
 *
 * @param conn Connection to undo the subscription on
 * @param name Event name
 * @return 0 on success, -1 on failure
 */
int rpc_connection_unsubscribe_event(rpc_connection_t conn, const char *name);

/**
 * Registers an event handler block for an event of a given name.
 *
 * Each time an event occurs, a handler block is going to be called.
 *
 * @param conn Connection to register an event handler for
 * @param name Name of an event to be handled
 * @param handler Event handler of rpc_handler_t type
 */
void *rpc_connection_register_event_handler(rpc_connection_t conn,
    const char *name, rpc_handler_t handler);

/**
 * Cancels further execution of a given event handler block for ongoing events
 * of a given name.
 *
 * @param conn Connection to remove event handler from
 * @param name Name of an event related to event handler
 * @param cookie Void pointer to event handler itself
 */
void rpc_connection_unregister_event_handler(rpc_connection_t conn,
    const char *name, void *cookie);

/**
 * Performs a synchronous RPC method call using a given connection.
 *
 * Function blocks until a result is ready and returns it, or cancels
 * and returns a NULL pointer if a timeout has occurred.
 *
 * Method call arguments need to be rpc_object_t instances, followed with
 * a NULL, denoting end of variable argument list.
 *
 * @param conn Connection to do a call on
 * @param method Name of a method to be called
 * @param ... Called method arguments
 * @return Result of the call
 */
rpc_object_t rpc_connection_call_sync(rpc_connection_t conn,
    const char *method, ...);

/**
 * Performs a synchronous RPC method call using a given connection.
 *
 * This function is similar to rpc_connection_call_sync(), but
 * instead of taking rpc_object_t arguments, it accepts a format
 * string and a list of values to pack, in format used by the
 * rpc_object_pack() function.
 *
 * @param conn Connection to do a call on
 * @param method Name of a method to be called
 * @param fmt Format strin
 * @param ... Called method arguments
 * @return Result of the call
 */
rpc_object_t rpc_connection_call_syncp(rpc_connection_t conn,
    const char *method, const char *fmt, ...);

/**
 * Performs a synchronous RPC method call using a given connection.
 *
 * Function blocks until a result is ready and returns it, or cancels
 * and returns a NULL pointer if a timeout has occurred.
 *
 * Instead of variable arguments length in rpc_connection_call() example,
 * this function takes previously assembled variable arguments list structure
 * as its argument.
 *
 * @param conn Connection to do a call on
 * @param method Name of a method to be called
 * @param ap Variable arguments list structure describing a method arguments
 * @return Result of the call
 */
rpc_object_t rpc_connection_call_syncv(rpc_connection_t conn,
    const char *method, va_list ap);

/**
 * Performs a RPC method call using a given connection.
 *
 * Function returns immediately without waiting for a RPC completion
 * and returns rpc_call_t object representing the ongoing call.
 *
 * Function supports a callback argument of rpc_callback_t type,
 * which is a pointer to a function to be called on RPC completion.
 * Can be set to NULL when that functionality is not needed by the caller.
 *
 * @param conn Connection to do a call on
 * @param name Name of a method to be called
 * @param args Variable length RPC method arguments list
 * @param callback Callback function pointer to be called on RPC completion
 * @return RPC call object
 */
rpc_call_t rpc_connection_call(rpc_connection_t conn, const char *name,
    rpc_object_t args, rpc_callback_t callback);

/**
 * Sends an event.
 *
 * @param conn Connection to send event across
 * @param name Event name
 * @param args Event arguments or NULL
 * @return 0 on success, -1 on failure
 */
int rpc_connection_send_event(rpc_connection_t conn, const char *name,
    rpc_object_t args);

/**
 * Ping the other end of a connection.
 */
int rpc_connection_ping(rpc_connection_t conn);

/**
 * Sets global event handler for a connection.
 *
 * @param conn Connection to set event handler for
 * @param handler Handler block
 */
void rpc_connection_set_event_handler(rpc_connection_t conn,
    rpc_handler_t handler);

/**
 * Sets global error handler for a connection.
 *
 * @param conn Connection to set error handler for
 * @param handler Error handler block
 */
void rpc_connection_set_error_handler(rpc_connection_t conn,
    rpc_error_handler_t handler);

const char *rpc_connection_get_remote_address(rpc_connection_t conn);

/**
 * Returns true if a connection has associated remote credentials information.
 *
 * @param conn Connection handle
 * @return true if credentials information is available, otherwise false
 */
bool rpc_connection_has_credentials(rpc_connection_t conn);

/**
 *
 * @param conn
 * @return
 */
uid_t rpc_connection_get_remote_uid(rpc_connection_t conn);

/**
 *
 * @param conn
 * @return
 */
gid_t rpc_connection_get_remote_gid(rpc_connection_t conn);

/**
 *
 * @param conn
 * @return
 */
pid_t rpc_connection_get_remote_pid(rpc_connection_t conn);

/**
 * Waits for a call to change status.
 *
 * @param call Call to wait on.
 * @return 0 on success, -1 on failure.
 */
int rpc_call_wait(rpc_call_t call);

/**
 * Requests a next chunk of a result from a call.
 *
 * When sync is set to true the function waits until the call finishes
 * and returns 1 if it has completed successfully - otherwise the function
 * returns 0.
 *
 * @param call Call to be continued
 * @param sync Synchronous continue flag
 * @return 1 for successfully completed RPC when sync flag was set, otherwise 0
 */
int rpc_call_continue(rpc_call_t call, bool sync);

/**
 * Aborts a pending call.
 *
 * @param call Call to be aborted
 * @return Function status - success is being reported as 0
 */
int rpc_call_abort(rpc_call_t call);

/**
 * Waits for a call to change status.
 *
 * If a timeout specified by a ts argument occurs, before a call
 * changes its status, function returns -1 value.
 *
 * @param call Call to wait on
 * @param ts Timeout value
 * @return 0 on success, -1 on failure or timeout
 */
int rpc_call_timedwait(rpc_call_t call, const struct timeval *ts);

/**
 * Checks whether a call has been completed successfully.
 *
 * @param call Call to be checked
 * @return 1 when call was successfully completed, otherwise 0
 */
int rpc_call_success(rpc_call_t call);

/**
 * Returns a current status of a given call
 * as an integer value castable to rpc_call_status_t.
 *
 * @param call Call to be checked
 * @return Call status
 */
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
