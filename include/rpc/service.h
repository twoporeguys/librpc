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

#ifndef LIBRPC_SERVICE_H
#define LIBRPC_SERVICE_H

#include <rpc/object.h>
#include <rpc/connection.h>

/**
 * @file service.h
 *
 * RPC service API.
 */


#ifdef __cplusplus
extern "C" {
#endif

#ifndef __APPLE__
#define	__unsafe_unretained
#endif

/**
 * Marks the function as "still running" even though the implementing
 * call returned.
 *
 *
 */
#define	RPC_FUNCTION_STILL_RUNNING	((rpc_object_t)1)

#define	RPC_DISCOVERABLE_INTERFACE	"com.twoporeguys.librpc.Discoverable"
#define	RPC_INTROSPECTABLE_INTERFACE	"com.twoporeguys.librpc.Introspectable"
#define	RPC_OBSERVABLE_INTERFACE	"com.twoporeguys.librpc.Observable"
#define	RPC_DEFAULT_INTERFACE		"com.twoporeguys.librpc.Default"

/**
 * RPC context structure.
 */
struct rpc_context;

/**
 * RPC instance structure.
 */
struct rpc_instance;

/**
 * RPC context handle.
 */
typedef struct rpc_context *rpc_context_t;

/**
 * RPC instance handle.
 */
typedef struct rpc_instance *rpc_instance_t;

/**
 * Method block type.
 */
typedef _Nullable rpc_object_t (^rpc_function_t)(void *_Nonnull cookie,
    _Nonnull rpc_object_t args);

/**
 * Method function type.
 */
typedef _Nullable rpc_object_t (*rpc_function_f)(void *_Nonnull cookie,
    _Nonnull rpc_object_t args);

/**
 * Property getter block type.
 */
typedef _Nullable rpc_object_t (^rpc_property_getter_t)(void *_Nonnull cookie);

/**
 * Property setter block type.
 */
typedef void (^rpc_property_setter_t)(void *_Nonnull cookie,
    _Nonnull rpc_object_t value);

/**
 * Asynchronous abort handler block type.
 */
typedef void (^rpc_abort_handler_t)(void);

/**
 * A macro to convert function pointer into @ref rpc_function_t block.
 */
#define	RPC_FUNCTION(_fn)						\
	^(void *_cookie, rpc_object_t _args) {				\
		return ((rpc_object_t)_fn(_cookie, _args));		\
	}

#define	RPC_PROPERTY_GETTER(_fn)					\
	^(void *_cookie) {						\
		return ((rpc_object_t)_fn(_cookie));			\
	}

#define	RPC_PROPERTY_SETTER(_fn)					\
	^(void *_cookie, rpc_object_t _value) {				\
		_fn(_cookie, _value);					\
	}

#define	RPC_ABORT_HANDLER(_fn, _arg)					\
	^(void *_cookie) {						\
		_fn(_arg);						\
	}

#define	RPC_EVENT(_name)						\
	{								\
		.rim_type = RPC_MEMBER_EVENT,				\
		.rim_name = (#_name)					\
	}

/**
 * A convenience macro to declare read-only property in the vtable array.
 */
#define	RPC_PROPERTY_RO(_name, _getter)					\
	{								\
		.rim_type = RPC_MEMBER_PROPERTY,			\
		.rim_name = (#_name),					\
		.rim_property = {					\
                        .rp_getter = RPC_PROPERTY_GETTER(_getter),	\
			.rp_setter = NULL,				\
			.rp_arg = NULL					\
                }							\
	}

/**
 * A convenience macro to declare write-only property in the vtable array.
 */
#define	RPC_PROPERTY_WO(_name, _setter)					\
	{								\
		.rim_type = RPC_MEMBER_PROPERTY,			\
		.rim_name = (#_name),					\
		.rim_property = {					\
                        .rp_getter = NULL,				\
			.rp_setter = RPC_PROPERTY_SETTER(_setter),	\
			.rp_arg = NULL					\
                }							\
	}

/**
 * A convenience macro to declare read-write property in the vtable array.
 */
#define	RPC_PROPERTY_RW(_name, _getter, _setter)			\
	{								\
		.rim_type = RPC_MEMBER_PROPERTY,			\
		.rim_name = (#_name),					\
		.rim_property = {					\
                        .rp_getter = RPC_PROPERTY_GETTER(_getter),	\
			.rp_setter = RPC_PROPERTY_SETTER(_setter),	\
			.rp_arg = NULL					\
                }							\
	}

/**
 * A convenience macro to declare RPC method in the vtable array.
 */
#define	RPC_METHOD(_name, _fn)						\
	{								\
		.rim_type = RPC_MEMBER_METHOD,				\
		.rim_name = (#_name),					\
		.rim_method = {						\
                        .rm_block = RPC_FUNCTION(_fn),			\
			.rm_arg = NULL					\
                }							\
	}

/**
 * Same as @ref RPC_METHOD, but takes a block instead of a function
 * pointer.
 */
#define	RPC_METHOD_BLOCK(_name, _block)					\
	{								\
		.rim_type = RPC_MEMBER_METHOD,				\
		.rim_name = (#_name),					\
		.rim_method = {						\
                        .rm_block = (_block),				\
                }							\
	}

#define	RPC_MEMBER_END {}

/**
 * Enumerates possible kinds of RPC interface members.
 */
enum rpc_if_member_type
{
	RPC_MEMBER_EVENT,		/**< Event member */
	RPC_MEMBER_PROPERTY,		/**< Property member */
	RPC_MEMBER_METHOD,		/**< Method member */
};

/**
 * Enumerates possible property right flags.
 */
enum rpc_property_rights
{
	RPC_PROPERTY_READ = (1 << 0),	/**< Property is readable */
	RPC_PROPERTY_WRITE = (1 << 1),	/**< Property is writable */
};


/**
 * Method descriptor.
 */
struct rpc_if_method
{
	__unsafe_unretained _Nonnull rpc_function_t rm_block;
	void *_Nullable	rm_arg;
};

/**
 * Property descriptor
 */
struct rpc_if_property
{
	__unsafe_unretained _Nullable rpc_property_getter_t rp_getter;
	__unsafe_unretained _Nullable rpc_property_setter_t rp_setter;
	void *_Nullable rp_arg;
};

/**
 * Interface member descriptor.
 *
 * Can be either a method, property or event.
 * */
struct rpc_if_member {
	const char *_Nonnull		rim_name;
        void *                          rim_interface;
	enum rpc_if_member_type		rim_type;
	union {
		struct rpc_if_method 	rim_method;
		struct rpc_if_property 	rim_property;
	};
};

/**
 * Creates a new RPC context.
 *
 * @return Newly created RPC context object.
 */
_Nonnull rpc_context_t rpc_context_create(void);

/**
 * Disposes existing RPC context and frees all associated resources.
 *
 * @param context Context to dispose
 */
void rpc_context_free(_Nonnull rpc_context_t context);

/**
 * Finds an instance registered in @p context. 
 *
 * If no path is given, returns the context default instance.
 *
 * @param context RPC context handle
 * @param path Instance path
 * @return RPC instance handle or NULL if not found
 */
_Nullable rpc_instance_t rpc_context_find_instance(
    _Nonnull rpc_context_t context, const char *_Nullable path);

/**
 * Returns root instance associated with @p context.
 *
 * @param context RPC context handle
 * @return RPC instance handle
 */
_Nonnull rpc_instance_t rpc_context_get_root(_Nonnull rpc_context_t context);

/**
 * Registers a new instance in @p context instance tree.
 *
 * @param context RPC context handle
 * @param instance RPC instance handle
 * @return 0 on success, -1 on error
 */
int rpc_context_register_instance(_Nonnull rpc_context_t context,
    _Nonnull rpc_instance_t instance);

/**
 *
 * @param context
 * @param path
 */
void rpc_context_unregister_instance(_Nonnull rpc_context_t context,
    const char *_Nonnull path);

/**
 * Registers a given rpc_method structure as an RPC method in a given context.
 *
 * @param context Target context.
 * @param m RPC method structure.
 * @return Status.
 */
int rpc_context_register_member(_Nonnull rpc_context_t context,
    const char *_Nullable interface, struct rpc_if_member *_Nonnull m);

/**
 * Registers a given block as a RPC method for a given context.
 *
 * @param context Target context.
 * @param name Method name.
 * @param descr Method description.
 * @param arg Method context.
 * @param func RPC method block.
 * @return Status.
 */
int rpc_context_register_block(_Nonnull rpc_context_t context,
    const char *_Nullable interface, const char *_Nonnull name,
    void *_Nullable arg, _Nonnull rpc_function_t func);

/**
 * Registers a given function as a RPC method for a given context.
 *
 * @param context Target context.
 * @param name Method name.
 * @param descr Method description.
 * @param arg Method context.
 * @param func RPC method function
 * @return Status.
 */
int rpc_context_register_func(_Nonnull rpc_context_t context,
    const char *_Nullable interface, const char *_Nonnull name,
    void *_Nullable arg, _Nonnull rpc_function_f func);

/**
 * Unregisters a given RPC method.
 *
 * @param context Target context.
 * @param name Method name.
 * @return Status.
 */
int rpc_context_unregister_member(_Nonnull rpc_context_t context,
    const char *_Nullable interface, const char *_Nonnull name);

/**
 * Installs a hook for every RPC function called.
 *
 * The hook will be called before an actual implementation of RPC function
 * gets called.
 *
 * @param context Target context
 * @param fn Hook function
 */
void rpc_context_set_pre_call_hook(_Nonnull rpc_context_t context,
    _Nonnull rpc_function_t fn);

/**
 * Installs a hook for every RPC function called.
 *
 * The hook will be called after an actual implementation of RPC function
 * is called.
 *
 * @param context Target context
 * @param fn Hook function
 */
void rpc_context_set_post_call_hook(_Nonnull rpc_context_t context,
    _Nonnull rpc_function_t fn);

/**
 *
 * @param context RPC context handle
 * @param name
 * @param args
 * @return
 */
_Nullable rpc_call_t rpc_context_dispatch_call(_Nonnull rpc_context_t context,
    const char *_Nonnull name, _Nullable rpc_object_t args);

/**
 * Notifies interested listeners of changes to the instance at @p path.
 *
 * If no path is specified the context default instance is assumed. If there is
 * no interface specified, RPC_DEFAULT_INTERFACE will be used.
 *
 * @param context
 * @param path
 * @param interface
 * @param name
 * @param args Data specific to the event
 */
void rpc_context_emit_event(_Nonnull rpc_context_t context,
    const char *_Nullable path, const char *_Nullable interface,
    const char *_Nonnull name, _Nonnull rpc_object_t args);

/**
 * Returns the argument associated with method.
 *
 * @param cookie Running call handle
 */
void *_Nullable rpc_function_get_arg(void *_Nonnull cookie);

/**
 * Returns the RPC context handle associated with currently executing
 * function.
 *
 * @param cookie Running call handle
 * @return RPC context handle
 */
_Nonnull rpc_context_t rpc_function_get_context(void *_Nonnull cookie);

/**
 * Returns the instance handle associated with currently executing function.
 *
 * @param cookie Running call handle
 * @return RPC context handle
 */
_Nonnull rpc_instance_t rpc_function_get_instance(void *_Nonnull cookie);

/**
 * Returns the called method name.
 *
 * @param cookie Running call handle
 */
const char *_Nonnull rpc_function_get_name(void *_Nonnull cookie);

/**
 * Returns the path method was called on or NULL.
 *
 * @param cookie Running call handle
 */
const char *_Nonnull rpc_function_get_path(void *_Nonnull cookie);

/**
 * Returns the called interface name or NULL.
 *
 * @param cookie Running call handle
 */
const char *_Nonnull rpc_function_get_interface(void *_Nonnull cookie);

/**
 * Sends a response to a call.
 *
 * This function may be called only once during the lifetime of a single
 * call (for a given cookie). When called, return value of a method
 * is silently ignored (it is preferred to return NULL).
 *
 * @param cookie Running call handle
 * @param object Response.
 */
void rpc_function_respond(void *_Nonnull cookie, _Nullable rpc_object_t object);

/**
 * Sends an error response to a call.
 *
 * This function may be called only once during the lifetime of a single
 * call (for a given cookie). When called, return value of a method
 * is silently ignored (it is preferred to return NULL).
 *
 * When called in a streaming function, implicitly ends streaming response.
 *
 * @param cookie Running call handle
 * @param code Error (errno) code
 * @param message Error message format
 * @param ... Format arguments
 */
void rpc_function_error(void *_Nonnull cookie, int code,
    const char *_Nonnull message, ...);

/**
 * Reports an exception for a given ongoing call identifier.
 *
 * @param cookie Running call handle
 * @param exception Exception object
 */
void rpc_function_error_ex(void *_Nonnull cookie,
    _Nonnull rpc_object_t exception);

/**
 * Generates a new value in a streaming response.
 *
 * @param cookie Running call handle
 * @param fragment Next data fragment
 * @return Status. Success is reported by returning 0
 */
int rpc_function_yield(void *_Nonnull cookie, _Nonnull rpc_object_t fragment);

/**
 * Ends a streaming response.
 *
 * When that function is called, sending further responses (either singular,
 * streaming or error responses) is not allowed. Return value of a method
 * functions is ignored.
 *
 * @param cookie Running call handle
 */
void rpc_function_end(void *_Nonnull cookie);

/**
 * Asynchronously abort a running call on the server.
 *
 * This function makes @ref rpc_function_should_abort return @p true
 * and a running @ref rpc_function_yield to return immediately with an error.
 *
 * @param cookie Running call handle
 */
void rpc_function_kill(void *_Nonnull cookie);

/**
 * Returns the value of a flag saying whether or not a method should
 * immediately stop because it was aborted on the client side.
 *
 * @param cookie Running call handle
 * @return Whether or not function should abort
 */
bool rpc_function_should_abort(void *_Nonnull cookie);

/**
 * Sets a callback to be called when running method got an abort signal
 * from the client.
 *
 * @param cookie Running call handle
 * @param handler Abort handling block
 */
void rpc_function_set_async_abort_handler(void *_Nonnull cookie,
    _Nullable rpc_abort_handler_t handler);

/**
 * Creates a new instance handle.
 *
 * @param path Instance path
 * @param arg User data pointer
 * @return
 */
_Nullable rpc_instance_t rpc_instance_new(void *_Nullable arg,
    const char *_Nonnull fmt, ...);

/**
 * Sets the description string of an instance.
 *
 * @param instance Instance handle
 * @param fmt Format string
 * @param ... Format arguments
 */
void rpc_instance_set_description(_Nonnull rpc_instance_t instance,
    const char *_Nonnull fmt, ...);

/**
 * Returns the user data pointer associated with @p instance.
 *
 * @param instance Instance handle
 * @return User data pointer
 */
void *_Nullable rpc_instance_get_arg(_Nonnull rpc_instance_t instance);

/**
 * Returns @p instance path.
 *
 * @param instance Instance handle
 * @return Instance path
 */
const char *_Nonnull rpc_instance_get_path(_Nonnull rpc_instance_t instance);

/**
 * Registers interface @p interface under @p instance.
 *
 * @param instance Instance handle
 * @param interface Interface name
 * @param vtable Member virtual table (vtable)
 * @param arg User data pointer
 * @return 0 on success, -1 on error
 */
int rpc_instance_register_interface(_Nonnull rpc_instance_t instance,
    const char *_Nonnull interface,
    const struct rpc_if_member *_Nullable vtable, void *_Nullable arg);

/**
 * Unregisters interface @p interface from @p instance along with all
 * interface members.
 *
 * @param instance Instance handle
 * @param interface Interface name
 */
void rpc_instance_unregister_interface(_Nonnull rpc_instance_t instance,
    const char *_Nonnull interface);

/**
 * Registers a single member of interface @p interface under @p instance.
 *
 * If no interface is specified, RPC_DEFAULT_INTERFACE will be assumed.
 * 
 * @param instance Intance handle
 * @param interface Interface name
 * @param member Member descriptor
 * @return 0 on success, -1 on error
 */
int rpc_instance_register_member(_Nonnull rpc_instance_t instance,
    const char *_Nullable interface,
    const struct rpc_if_member *_Nonnull member);

/**
 * Unregisters a previously registered member named @p name from interface
 * @p interface on @p instance.
 *
 * If no interface is specified, RPC_DEFAULT_INTERFACE will be assumed.
 * 
 * @param instance Instance handle
 * @param interface Interface name
 * @param name Member name
 * @return 0 on success, -1 on error
 */
int rpc_instance_unregister_member(_Nonnull rpc_instance_t instance,
    const char *_Nullable interface, const char *_Nonnull name);

/**
 * Registers a new method called @p name on interface @p interface under
 * instance @p instance.
 *
 * If no interface is specified, RPC_DEFAULT_INTERFACE will be assumed.
 * 
 * @param instance Instance handle
 * @param interface Interface name
 * @param name Method name
 * @param arg Method private data pointer
 * @param fn Block
 * @return 0 on success, -1 on error
 */
int rpc_instance_register_block(_Nonnull rpc_instance_t instance,
    const char *_Nullable interface, const char *_Nonnull name,
    void *_Nullable arg, _Nonnull rpc_function_t fn);

/**
 * Same as @ref rpc_instance_register_block, but takes a function pointer
 * instead.
 *
 * @param instance Instance handle
 * @param interface Interface name
 * @param name Method name
 * @param arg Method private data pointer
 * @param fn Function pointer
 */
int rpc_instance_register_func(_Nonnull rpc_instance_t instance,
    const char *_Nullable interface, const char *_Nonnull name,
    void *_Nullable arg, _Nonnull rpc_function_f fn);

/**
 * Finds member called @p name belonging to a @p interface in @p instance.
 *
 * If no interface is specified, RPC_DEFAULT_INTERFACE will be assumed.
 * 
 * @param instance Instance handle
 * @param interface Interface name
 * @param name Member name
 * @return @ref rpc_if_member structure pointer or @p NULL if not found.
 */
struct rpc_if_member *_Nullable rpc_instance_find_member(
    _Nonnull rpc_instance_t instance, const char *_Nullable interface,
    const char *_Nonnull name);

/**
 * Tells whether or not @p instance implements interface @p interface.
 *
 * @param instance Instance handle
 * @param interface Interface name
 * @return @p true if implemented, otherwise @p false
 */
bool rpc_instance_has_interface(_Nonnull rpc_instance_t instance,
    const char *_Nonnull interface);

/**
 * Notifies interested listeners of changes to the instance.
 *
 * If no interface is specified, RPC_DEFAULT_INTERFACE will be assumed.
 * 
 * @param instance Instance handle
 * @param interface Interface name
 * @param name Event name
 * @param args Data specific to the event
 */
void rpc_instance_emit_event(_Nonnull rpc_instance_t instance,
    const char *_Nullable interface, const char *_Nonnull name,
    _Nonnull rpc_object_t args);

/**
 * Registers property named @p name on interface @p interface under instance
 * @p instance.
 *
 * If no interface is specified, RPC_DEFAULT_INTERFACE will be assumed.
 *
 * The property can be:
 * - read-only, when getter is non-NULL and setter is NULL
 * - write-only, when getter is NULL and setter is non-NULL
 * - read-write, when both getter and setter blocks are provided.
 *
 * @param instance Instance handle
 * @param interface Interface name
 * @param name Property name
 * @param arg User data pointer
 * @param getter Getter block or NULL if write-only
 * @param setter Setter block or NULL if read-nly
 * @return 0 on success, -1 on error
 */
int rpc_instance_register_property(_Nonnull rpc_instance_t instance,
    const char *_Nullable interface, const char *_Nonnull name,
    void *_Nullable arg, _Nullable rpc_property_getter_t getter,
    _Nullable rpc_property_setter_t setter);

/**
 * Returns access rights of property @p name of interface @p interface
 * implemented in instance @p instance.
 *
 * If no interface is specified, RPC_DEFAULT_INTERFACE will be assumed.
 * 
 * @param instance Instance handle
 * @param interface Interface name
 * @param name Property name
 * @return
 */
int rpc_instance_get_property_rights(_Nonnull rpc_instance_t instance,
    const char *_Nullable interface, const char *_Nonnull name);

/**
 *
 * @param instance Instance handle
 * @param interface Interface name
 * @param name
 * @return
 */
int rpc_instance_register_event(_Nonnull rpc_instance_t instance,
    const char *_Nonnull interface, const char *_Nonnull name);

/**
 * Notifies the librpc layer that property value has changed.
 *
 * This function is used to notify remote property listeners that the
 * value might have changed.
 *
 * If @p value is @p NULL, then librpc will internally query the getter
 * for the value.
 *
 * If no interface is specified, RPC_DEFAULT_INTERFACE will be assumed.
 *
 * @param instance Instance handle
 * @param interface Interface name
 * @param name Property name
 * @param value New property value
 */
void rpc_instance_property_changed(_Nonnull rpc_instance_t instance,
    const char *_Nonnull interface, const char *_Nonnull name,
    _Nullable rpc_object_t value);

/**
 * Returns instance associated with the getter or setter call.
 *
 * @param cookie Running call identifier
 * @return Instance handle
 */
_Nonnull rpc_instance_t rpc_property_get_instance(void *_Nonnull cookie);

/**
 * Returns the user data pointer associated with the currently running
 * getter/setter.
 *
 * @param cookie Property call handle
 * @return User data pointer
 */
void *_Nullable rpc_property_get_arg(void *_Nonnull cookie);

/**
 * Indicate that the current getter or setter run should generate an error.
 *
 * After using this function, return value from the getter is ignored.
 *
 * @param cookie Property call handle
 * @param code Error code
 * @param fmt Message format string
 * @param ... Format string arguments
 */
void rpc_property_error(void *_Nonnull cookie, int code,
    const char *_Nonnull fmt, ...);

/**
 * Releases instance handle.
 *
 * @param instance Instance handle
 */
void rpc_instance_free(_Nonnull rpc_instance_t instance);

#ifdef __cplusplus
}
#endif

#endif //LIBRPC_SERVICE_H
