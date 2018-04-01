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
#ifdef LIBDISPATCH_SUPPORT
#include <dispatch/dispatch.h>
#endif

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

#define RPC_NULL_FORMAT "[]"

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
	RPC_TRANSPORT_ERROR,		/**< Transport-specific error */
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
	RPC_CALL_ABORTED,		/**< Call was aborted by the user */
	RPC_CALL_ENDED			/**< Streaming call ended */
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
typedef void (^rpc_handler_t)(const char *_Nullable path,
    const char *_Nullable interface, const char *_Nonnull name,
    _Nonnull rpc_object_t args);

/**
 * Definition of RPC property change handler block type.
 */
typedef void (^rpc_property_handler_t)(_Nonnull rpc_object_t value);

/**
 * Definition of RPC error handler block type.
 */
typedef void (^rpc_error_handler_t)(rpc_error_code_t code,
    _Nullable rpc_object_t args);

/**
 * Definition of RPC callback block type.
 */
typedef bool (^rpc_callback_t)(_Nonnull rpc_call_t call);

/**
 * Converts function pointer to a @ref rpc_handler_t block type.
 */
#define	RPC_HANDLER(_fn, _arg) 						\
	^(const char *_path, const char *_iface, const char *_name, 	\
	    rpc_object_t _args) {					\
		_fn(_arg, _path, _iface, _name, _args);			\
	}

/**
 * Converts function pointer to a @ref rpc_property_handler_t block type.
 */
#define	RPC_PROPERTY_HANDLER(_fn, _arg)					\
	^(rpc_object_t _value) {					\
		_fn(_arg, _value);					\
	}

/**
 * Converts function pointer to a @ref rpc_error_handler_t block type.
 */
#define	RPC_ERROR_HANDLER(_fn, _arg) 					\
	^(rpc_error_code_t _code, rpc_object_t _args) {			\
		_fn(_arg, _code, _args);				\
	}

/**
 * Converts function pointer to a @ref rpc_callback_t block type.
 */
#define	RPC_CALLBACK(_fn, _arg) 					\
	^(rpc_object_t _args, rpc_call_status_t _status) {		\
		return ((bool)_fn(_arg, _args, _status));		\
	}

/**
 * Creates a new connection from the provided opaque cookie.
 *
 * @param cookie Opaque data
 * @param params Transport-specific parameters
 * @return Connection handle or NULL on failure
 */
_Nullable rpc_connection_t rpc_connection_create(void *_Nonnull cookie,
    _Nullable rpc_object_t params);

/**
 * Closes the connection and frees all resources associated with it.
 *
 * @param conn Connection to close
 * @return 0 on success, -1 on failure
 */
int rpc_connection_close(_Nonnull rpc_connection_t conn);

/**
 * Returns @p true if connection is open, otherwise @p false.
 *
 * @param conn Connection handle
 * @return @p true if connection is open, otherwise @p false
 */
bool rpc_connection_is_open(_Nonnull rpc_connection_t conn);

/**
 * Frees resources associated with @ref rpc_connection_t.
 *
 * @param conn Connection handle
 */
void rpc_connection_free(_Nonnull rpc_connection_t conn);

#ifdef LIBDISPATCH_SUPPORT
/**
 * Assigns a libdispatch queue to the connection.
 *
 * Once called, all callbacks invoked by a connection (asynchronous call
 * callbacks or event callbacks) will be pushed to @p queue instead of being
 * serviced by internal glib thread pool.
 *
 * @param conn Connection handle
 * @param queue libdispatch queue
 * @return 0 on success, -1 on failure
 */
int rpc_connection_set_dispatch_queue(_Nonnull rpc_connection_t conn,
    _Nonnull dispatch_queue_t queue);
#endif

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
int rpc_connection_subscribe_event(_Nonnull rpc_connection_t conn,
    const char *_Nullable path, const char *_Nullable interface,
    const char *_Nonnull name);

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
int rpc_connection_unsubscribe_event(_Nonnull rpc_connection_t conn,
    const char *_Nullable path, const char *_Nullable interface,
    const char *_Nonnull name);

/**
 * Registers an event handler block for an event of a given name.
 *
 * Each time an event occurs, a handler block is going to be called.
 *
 * @param conn Connection to register an event handler for
 * @param name Name of an event to be handled
 * @param handler Event handler of rpc_handler_t type
 */
void *_Nonnull rpc_connection_register_event_handler(
    _Nonnull rpc_connection_t conn, const char *_Nullable path,
    const char *_Nullable interface, const char *_Nonnull name,
    _Nullable rpc_handler_t handler);

/**
 * Cancels further execution of a given event handler block for ongoing events
 * of a given name.
 *
 * @param conn Connection to remove event handler from
 * @param cookie Void pointer to event handler itself
 */
void rpc_connection_unregister_event_handler(_Nonnull rpc_connection_t conn,
    void *_Nonnull cookie);

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
_Nullable rpc_object_t rpc_connection_call_sync(_Nonnull rpc_connection_t conn,
    const char *_Nullable path, const char *_Nullable interface,
    const char *_Nonnull method, ...);

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
_Nullable rpc_object_t rpc_connection_call_syncv(_Nonnull rpc_connection_t conn,
    const char *_Nullable path, const char *_Nullable interface,
    const char *_Nonnull method, va_list ap);

/**
 * Performs a synchronous RPC method call using a given connection.
 *
 * This function is similar to rpc_connection_call_sync(), but
 * instead of taking rpc_object_t arguments, it accepts a format
 * string and a list of values to pack, in format used by the
 * rpc_object_pack() function.
 *
 * @param conn Connection handle
 * @param method Name of a method to be called
 * @param fmt Format string
 * @param ... Called method arguments
 * @return Result of the call
 */
_Nullable rpc_object_t rpc_connection_call_syncp(_Nonnull rpc_connection_t conn,
    const char *_Nullable path, const char *_Nullable interface,
    const char *_Nonnull method, const char *_Nonnull fmt, ...);

/**
 * A variation of @ref rpc_connection_call_syncp that takes a @p va_list
 * instead of the variable argument list.
 *
 * @param conn Connection handle
 * @param path
 * @param interface
 * @param method
 * @param fmt
 * @param ap
 * @return
 */
_Nullable rpc_object_t rpc_connection_call_syncpv(
    _Nonnull rpc_connection_t conn, const char *_Nullable path,
    const char *_Nullable interface, const char *_Nonnull method,
    const char *_Nonnull fmt, va_list ap);

/**
 * Performs a synchronous RPC function call using a given connection.
 *
 * Function blocks until a result is ready and returns it, or cancels
 * and returns a NULL pointer if a timeout has occurred.
 *
 * This function can be only used to call pure functions (not operating
 * on objects, that is, like rpc_connection_call_syncp() but with path
 * and interface parameters set to NULL). Set fmt to  RPC_NULL_FORMAT
 * if there are no arguments.
 *
 * @param name
 * @param fmt
 * @param ...
 * @return
 */
_Nullable rpc_object_t rpc_connection_call_simple(
    _Nonnull rpc_connection_t conn, const char *_Nonnull name,
    const char *_Nonnull fmt, ...);

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
_Nullable rpc_call_t rpc_connection_call(_Nonnull rpc_connection_t conn,
    const char *_Nullable path, const char *_Nullable interface,
    const char *_Nonnull name, _Nullable rpc_object_t args,
    _Nullable rpc_callback_t callback);

/**
 *
 * @param conn
 * @param path
 * @param interface
 * @param name
 * @return
 */
_Nullable rpc_object_t rpc_connection_get_property(
    _Nonnull rpc_connection_t conn, const char *_Nullable path,
    const char *_Nullable interface, const char *_Nonnull name);

/**
 *
 * @param conn
 * @param path
 * @param interface
 * @param name
 * @param value
 * @return
 */
_Nullable rpc_object_t rpc_connection_set_property(
    _Nonnull rpc_connection_t conn, const char *_Nullable path,
    const char *_Nullable interface, const char *_Nonnull name,
    rpc_object_t _Nonnull value);


/**
 *
 * @param conn
 * @param path
 * @param interface
 * @param name
 * @param fmt
 * @param ...
 * @return
 */
_Nullable rpc_object_t rpc_connection_set_propertyp(
    _Nonnull rpc_connection_t conn, const char *_Nullable path,
    const char *_Nullable interface, const char *_Nonnull name,
    const char *_Nonnull fmt, ...);

/**
 *
 * @param conn
 * @param path
 * @param interface
 * @param name
 * @param fmt
 * @param ap
 * @return
 */
_Nullable rpc_object_t rpc_connection_set_propertypv(
    _Nonnull rpc_connection_t conn, const char *_Nullable path,
    const char *_Nonnull interface, const char *_Nonnull name,
    const char *_Nonnull fmt, va_list ap);

/**
 *
 * @param conn
 * @param path
 * @param interface
 * @param property
 * @param handler
 * @return
 */
void *_Nullable rpc_connection_watch_property(
    _Nonnull rpc_connection_t conn, const char *_Nullable path,
    const char *_Nullable interface, const char *_Nonnull property,
    _Nonnull rpc_property_handler_t handler);

/**
 * Sends an event.
 *
 * @param conn Connection to send event across
 * @param name Event name
 * @param args Event arguments or NULL
 * @return 0 on success, -1 on failure
 */
int rpc_connection_send_event(_Nonnull rpc_connection_t conn,
    const char *_Nullable path, const char *_Nullable interface,
    const char *_Nonnull name, _Nonnull rpc_object_t args);

/**
 * Ping the other end of a connection.
 */
int rpc_connection_ping(_Nonnull rpc_connection_t conn);

/**
 * Sets global event handler for a connection.
 *
 * @param conn Connection to set event handler for
 * @param handler Handler block
 */
void rpc_connection_set_event_handler(_Nonnull rpc_connection_t conn,
    _Nullable rpc_handler_t handler);

/**
 * Sets global error handler for a connection.
 *
 * @param conn Connection to set error handler for
 * @param handler Error handler block
 */
void rpc_connection_set_error_handler(_Nonnull rpc_connection_t conn,
    _Nullable rpc_error_handler_t handler);

/**
 *
 * @param conn
 * @return
 */
const char *_Nullable rpc_connection_get_remote_address(
    _Nonnull rpc_connection_t conn);

/**
 * Returns true if a connection has associated remote credentials information.
 *
 * @param conn Connection handle
 * @return true if credentials information is available, otherwise false
 */
bool rpc_connection_has_credentials(_Nonnull rpc_connection_t conn);

/**
 * Gets remote side UID.
 *
 * @param conn Connection handle
 * @return
 */
uid_t rpc_connection_get_remote_uid(_Nonnull rpc_connection_t conn);

/**
 *
 * @param conn Connection handle
 * @return
 */
gid_t rpc_connection_get_remote_gid(_Nonnull rpc_connection_t conn);

/**
 *
 * @param conn Connection handle
 * @return
 */
pid_t rpc_connection_get_remote_pid(_Nonnull rpc_connection_t conn);

/**
 * Waits for a call to change status.
 *
 * @param call Call to wait on.
 * @return 0 on success, -1 on failure.
 */
int rpc_call_wait(_Nonnull rpc_call_t call);

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
int rpc_call_continue(_Nonnull rpc_call_t call, bool sync);

/**
 * Aborts a pending call.
 *
 * @param call Call to be aborted
 * @return Function status - success is being reported as 0
 */
int rpc_call_abort(_Nonnull rpc_call_t call);

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
int rpc_call_timedwait(_Nonnull rpc_call_t call,
    const struct timeval *_Nonnull ts);

/**
 * Checks whether a call has been completed successfully.
 *
 * @param call Call to be checked
 * @return 1 when call was successfully completed, otherwise 0
 */
int rpc_call_success(_Nonnull rpc_call_t call);

/**
 * Returns a current status of a given call
 * as an integer value castable to rpc_call_status_t.
 *
 * @param call Call to be checked
 * @return Call status
 */
int rpc_call_status(_Nonnull rpc_call_t call);

/**
 * Returns a call result (or a current fragment).
 *
 * @param call Call to get result from
 * @return Result
 */
_Nullable rpc_object_t rpc_call_result(_Nonnull rpc_call_t call);

/**
 * Frees a rpc_call_t object.
 *
 * @param call Call to free
 */
void rpc_call_free(_Nonnull rpc_call_t call);

#ifdef __cplusplus
}
#endif

#endif //LIBRPC_CONNECTION_H
