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

#define	RPC_FUNCTION_STILL_RUNNING	((rpc_object_t)1)

#define	RPC_DISCOVERABLE_INTERFACE	"com.twoporeguys.librpc.Discoverable"
#define	RPC_INTROSPECTABLE_INTERFACE	"com.twoporeguys.librpc.Introspectable"
#define	RPC_OBSERVABLE_INTERFACE	"com.twoporeguys.librpc.Observable"
#define	RPC_DEFAULT_INTERFACE		"com.twoporeguys.librpc.Default"

/**
 * RPC context structure definition.
 */
struct rpc_context;

/**
 * RPC context structure pointer definition.
 */
typedef struct rpc_context *rpc_context_t;

struct rpc_instance;
typedef struct rpc_instance *rpc_instance_t;

/**
 * Definition of RPC method block type.
 */
typedef _Nullable rpc_object_t (^rpc_function_t)(void *_Nonnull cookie,
    _Nonnull rpc_object_t args);

/**
 * Definition of RPC method function type.
 */
typedef _Nullable rpc_object_t (*rpc_function_f)(void *_Nonnull cookie,
    _Nonnull rpc_object_t args);

/**
 *
 */
typedef _Nullable rpc_object_t (^rpc_property_getter_t)(void *_Nonnull cookie);

/**
 *
 */
typedef void (^rpc_property_setter_t)(void *_Nonnull cookie,
    _Nonnull rpc_object_t value);

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

#define	RPC_EVENT(_name)						\
	{								\
		.rim_type = RPC_MEMBER_EVENT,				\
		.rim_name = (#_name)					\
	}

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

#define	RPC_METHOD(_name, _fn)						\
	{								\
		.rim_type = RPC_MEMBER_METHOD,				\
		.rim_name = (#_name),					\
		.rim_method = {						\
                        .rm_block = RPC_FUNCTION(_fn),			\
			.rm_arg = NULL					\
                }							\
	}

#define	RPC_METHOD_BLOCK(_name, _block)					\
	{								\
		.rim_type = RPC_MEMBER_METHOD,				\
		.rim_name = (#_name),					\
		.rim_method = {						\
                        .rm_block = (_block),				\
                }							\
	}

#define	RPC_MEMBER_END {}

enum rpc_if_member_type
{
	RPC_MEMBER_EVENT,
	RPC_MEMBER_PROPERTY,
	RPC_MEMBER_METHOD,
};

enum rpc_property_rights
{
	RPC_PROPERTY_READ = (1 << 0),
	RPC_PROPERTY_WRITE = (1 << 1),
};


/**
 * RPC method descriptor.
 */
struct rpc_if_method
{
	_Nonnull rpc_function_t		rm_block;
	void *_Nullable			rm_arg;
};

struct rpc_if_property
{
	_Nullable rpc_property_getter_t	rp_getter;
	_Nullable rpc_property_setter_t	rp_setter;
	void *_Nullable			rp_arg;
};

struct rpc_if_member {
	const char *_Nonnull		rim_name;
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
 *
 * @param context
 * @param path
 * @return
 */
_Nullable rpc_instance_t rpc_context_find_instance(
    _Nonnull rpc_context_t context, const char *_Nonnull path);

/**
 *
 * @param context
 * @return
 */
_Nonnull rpc_instance_t rpc_context_get_root(_Nonnull rpc_context_t context);

/**
 * Registers a new object under context's object tree.
 *
 * @param context
 * @param path
 * @param instance
 * @return
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
    const char *_Nonnull interface, const char *_Nonnull name);

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
 * @param context
 * @param name
 * @param args
 * @return
 */
_Nullable rpc_call_t rpc_context_dispatch_call(_Nonnull rpc_context_t context,
    const char *_Nonnull name, _Nullable rpc_object_t args);

/**
 *
 * @param context
 * @param path
 * @param interface
 * @param name
 */
void rpc_context_emit_event(_Nonnull rpc_context_t context,
    const char *_Nullable path, const char *_Nullable interface,
    const char *_Nonnull name, _Nonnull rpc_object_t args);

/**
 * Returns the argument associated with method.
 *
 * @param cookie Running call identifier.
 */
void *_Nullable rpc_function_get_arg(void *_Nonnull cookie);

/**
 * Returns the RPC context object associated with currently executing
 * function.
 *
 * @param cookie Running call identifier
 * @return RPC context
 */
_Nonnull rpc_context_t rpc_function_get_context(void *_Nonnull cookie);

/**
 *
 * @param cookie
 * @return
 */
_Nonnull rpc_instance_t rpc_function_get_instance(void *_Nonnull cookie);

/**
 * Returns the called method name.
 *
 * @param cookie Running call identifier.
 */
const char *_Nonnull rpc_function_get_name(void *_Nonnull cookie);

/**
 * Returns the path method was called on or NULL.
 *
 * @param cookie Running call identifier.
 */
const char *_Nonnull rpc_function_get_path(void *_Nonnull cookie);

/**
 * Returns the called interface name or NULL.
 *
 * @param cookie Running call identifier.
 */
const char *_Nonnull rpc_function_get_interface(void *_Nonnull cookie);

/**
 * Sends a response to a call.
 *
 * This function may be called only once during the lifetime of a single
 * call (for a given cookie). When called, return value of a method
 * is silently ignored (it is preferred to return NULL).
 *
 * @param cookie Running call identifier.
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
 * @param cookie Running call identifier.
 * @param code Error (errno) code.
 * @param message Error message format.
 * @param ... Format arguments.
 */
void rpc_function_error(void *_Nonnull cookie, int code,
    const char *_Nonnull message, ...);

/**
 * Reports an exception for a given ongoing call identifier.
 *
 * @param cookie Running call identifier.
 * @param exception Exception data.
 */
void rpc_function_error_ex(void *_Nonnull cookie,
    _Nonnull rpc_object_t exception);

/**
 * Generates a new value in a streaming response.
 *
 * @param cookie Running call identifier.
 * @param fragment Next data fragment.
 * @return Status. Success is reported by returning 0.
 */
int rpc_function_yield(void *_Nonnull cookie, _Nonnull rpc_object_t fragment);

/**
 * Ends a streaming response.
 *
 * When that function is called, sending further responses (either singular,
 * streaming or error responses) is not allowed. Return value of a method
 * functions is ignored.
 *
 * @param cookie Running call identifier.
 */
void rpc_function_end(void *_Nonnull cookie);

/**
 * Returns the value of a flag saying whether or not a method should
 * immediately stop because it was aborted on the client side.
 *
 * @param cookie Running call identifier.
 * @return Whether or not function should abort.
 */
bool rpc_function_should_abort(void *_Nonnull cookie);

/**
 *
 * @param path
 * @param arg
 * @return
 */
_Nullable rpc_instance_t rpc_instance_new(void *_Nullable arg,
    const char *_Nonnull fmt, ...);

/**
 *
 * @param fmt
 * @param ...
 */
void rpc_instance_set_description(_Nonnull rpc_instance_t,
    const char *_Nonnull fmt, ...);

/**
 *
 * @param instance
 * @return
 */
void *_Nullable rpc_instance_get_arg(_Nonnull rpc_instance_t instance);

/**
 * Returns the object path associated with the instance.
 *
 * @param instance Instance handle
 * @return Object path
 */
const char *_Nonnull rpc_instance_get_path(_Nonnull rpc_instance_t instance);

/**
 *
 * @param instance
 * @param interface
 * @param name
 * @param fn
 */
int rpc_instance_register_interface(_Nonnull rpc_instance_t instance,
    const char *_Nonnull interface,
    const struct rpc_if_member *_Nullable vtable, void *_Nullable arg);

/**
 *
 * @param instance
 * @param interface
 * @param member
 * @return
 */
int rpc_instance_register_member(_Nonnull rpc_instance_t instance,
    const char *_Nonnull interface,
    const struct rpc_if_member *_Nonnull member);

/**
 *
 * @param instance
 * @param interface
 * @param name
 * @return
 */
int rpc_instance_unregister_member(_Nonnull rpc_instance_t instance,
    const char *_Nonnull interface, const char *_Nonnull name);

/**
 *
 * @param instance
 * @param interface
 * @param name
 * @param arg
 * @param fn
 */
int rpc_instance_register_block(_Nonnull rpc_instance_t instance,
    const char *_Nonnull interface, const char *_Nonnull name,
    void *_Nullable arg, _Nonnull rpc_function_t fn);

/**
 *
 * @param instance
 * @param interface
 * @param name
 * @param fn
 */
int rpc_instance_register_func(_Nonnull rpc_instance_t instance,
    const char *_Nonnull interface, const char *_Nonnull name,
    void *_Nullable arg, _Nonnull rpc_function_f fn);

/**
 * Finds a given method belonging to a given interface in instance.
 *
 * @param instance
 * @param interface
 * @param name
 * @return
 */
struct rpc_if_member *_Nullable rpc_instance_find_member(
    _Nonnull rpc_instance_t instance, const char *_Nullable interface,
    const char *_Nonnull name);

/**
 *
 * @param interface
 * @return
 */
bool rpc_instance_has_interface(_Nonnull rpc_instance_t instance,
    const char *_Nonnull interface);

/**
 *
 * @param instance
 * @param interface
 * @param name
 */
void rpc_instance_emit_event(_Nonnull rpc_instance_t instance,
    const char *_Nonnull interface, const char *_Nonnull name,
    _Nonnull rpc_object_t args);

/**
 *
 * @param instance
 * @param name
 * @param value
 * @return
 */
int rpc_instance_register_property(_Nonnull rpc_instance_t instance,
    const char *_Nonnull interface, const char *_Nonnull name,
    void *_Nullable arg, _Nullable rpc_property_getter_t getter,
    _Nullable rpc_property_setter_t setter);

/**
 *
 * @param instance
 * @param name
 * @return
 */
int rpc_instance_get_property_rights(_Nonnull rpc_instance_t instance,
    const char *_Nonnull interface, const char *_Nonnull name);

/**
 * 
 * @param interface
 * @param name
 * @return
 */
int rpc_instance_register_event(_Nonnull rpc_instance_t instance,
    const char *_Nonnull interface, const char *_Nonnull name);

/**
 *
 * @param instance
 * @param interface
 * @param name
 */
void rpc_instance_property_changed(_Nonnull rpc_instance_t instance,
    const char *_Nonnull interface, const char *_Nonnull name,
    _Nonnull rpc_object_t value);

/**
 *
 * @param cookie
 * @return
 */
_Nonnull rpc_instance_t rpc_property_get_instance(void *_Nonnull cookie);

/**
 * Returns the user data pointer associated with the property.
 *
 * @param cookie
 * @return
 */
void *_Nullable rpc_property_get_arg(void *_Nonnull cookie);

/**
 *
 * @param cookie
 * @param code
 * @param fmt
 * @param ...
 */
void rpc_property_error(void *_Nonnull cookie, int code,
    const char *_Nonnull fmt, ...);

/**
 *
 * @param instance
 */
void rpc_instance_free(_Nonnull rpc_instance_t instance);

#ifdef __cplusplus
}
#endif

#endif //LIBRPC_SERVICE_H
