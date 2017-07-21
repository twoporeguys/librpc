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
 */


#ifndef LIBRPC_QUERY_H
#define LIBRPC_QUERY_H

#include "object.h"

struct rpc_query_iter;
typedef struct rpc_query_iter *rpc_query_iter_t;

typedef rpc_object_t (^rpc_query_cb_t)(rpc_object_t object);

#define	RPC_QUERY_CB(_fn, _arg)						\
	^(rpc_object_t _object) {					\
                return ((rpc_object_t)_fn(_arg, _object));		\
        }

struct rpc_query_params {
	bool 			single;
	bool			count;
	uint64_t 		offset;
	uint64_t 		limit;
	rpc_array_cmp_t 	sort;
	bool			reverse;
	rpc_query_cb_t 		callback;
};

typedef struct rpc_query_params *rpc_query_params_t;

rpc_object_t rpc_query_get(rpc_object_t object, const char *path,
    rpc_object_t default_val);
void rpc_query_set(rpc_object_t object, const char *path, rpc_object_t value,
    bool steal);
void rpc_query_delete(rpc_object_t object, const char *path);
bool rpc_query_contains(rpc_object_t object, const char *path);

rpc_query_iter_t rpc_query(rpc_object_t object, rpc_query_params_t params,
    rpc_object_t rules);
rpc_query_iter_t rpc_query_fmt(rpc_object_t object, rpc_query_params_t params,
    const char *rules_fmt, ...);
rpc_object_t rpc_query_apply(rpc_object_t object, rpc_object_t rules);
bool rpc_query_next(rpc_query_iter_t iter, rpc_object_t *chunk);
void rpc_query_iter_free(rpc_query_iter_t iter);

#endif //LIBRPC_QUERY_H
