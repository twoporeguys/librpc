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

struct rpc_context;

typedef struct rpc_context *rpc_context_t;
typedef rpc_object_t (^rpc_function_t)(void *cookie, rpc_object_t args);
typedef rpc_object_t (*rpc_function_f)(void *cookie, rpc_object_t args);

rpc_context_t rpc_context_create(void);
void rpc_context_free(rpc_context_t context);
int rpc_context_register_method(rpc_context_t context, const char *name,
    rpc_function_t func, void *arg, int flags);
int rpc_context_register_method_f(rpc_context_t context, const char *name,
    rpc_function_f func, void *arg, int flags);

void rpc_function_respond(void *cookie, rpc_object_t object);
void rpc_function_error(void *cookie, int code, const char *message, ...);
void rpc_function_error_ex(void *cookie, rpc_object_t exception);
void rpc_function_yield(void *cookie, rpc_object_t fragment);
void rpc_function_end(void *cookie);

#endif //LIBRPC_SERVICE_H
