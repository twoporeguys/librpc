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

struct rpc_context;

typedef struct rpc_context *rpc_context_t;
typedef rpc_object_t (^rpc_function_t)(void *cookie, rpc_object_t args);
typedef rpc_object_t (*rpc_function_f)(void *cookie, rpc_object_t args);

struct rpc_method
{
	const char *		rm_name;
	const char *		rm_description;
	rpc_function_t  	rm_block;
	void *			rm_arg;
};

/**
 * Creates a new RPC context.
 *
 * @return Newly created RPC context object.
 */
rpc_context_t rpc_context_create(void);

/**
 * Disposes existing RPC context and frees all associated resources.
 *
 * @param context Context to dispose
 */
void rpc_context_free(rpc_context_t context);
struct rpc_method *rpc_context_find_method(rpc_context_t, const char *name);
int rpc_context_register_method(rpc_context_t context, struct rpc_method *m);
int rpc_context_register_block(rpc_context_t context, const char *name,
    const char *descr, void *arg, rpc_function_t func);
int rpc_context_register_func(rpc_context_t context, const char *name,
    const char *descr, void *arg, rpc_function_f func);
int rpc_context_unregister_method(rpc_context_t context, const char *name);

rpc_call_t rpc_context_dispatch_call(rpc_context_t context, const char *name,
    rpc_object_t args);

/**
 * Returns the argument associated with metod.
 *
 * @param cookie
 * @return
 */
void *rpc_function_get_arg(void *cookie);

/**
 * Sends a response to a call.
 *
 * @param cookie Running call identifier
 * @param object Response
 */
void rpc_function_respond(void *cookie, rpc_object_t object);

/**
 * Sends an error response to a call.
 *
 * @param cookie
 * @param code
 * @param message
 * @param ...
 */
void rpc_function_error(void *cookie, int code, const char *message, ...);
void rpc_function_error_ex(void *cookie, rpc_object_t exception);
int rpc_function_yield(void *cookie, rpc_object_t fragment);
void rpc_function_end(void *cookie);
bool rpc_function_should_abort(void *cookie);

#ifdef __cplusplus
}
#endif

#endif //LIBRPC_SERVICE_H
