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

#ifndef LIBRPC_SERVER_H
#define LIBRPC_SERVER_H

#include <stdbool.h>
#include <rpc/service.h>
#include <rpc/connection.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rpc_server;

typedef struct rpc_server *rpc_server_t;
typedef void (^rpc_server_event_handler_t)(rpc_connection_t source,
    const char *name, rpc_object_t args);
typedef void (*rpc_server_event_handler_f)(rpc_connection_t source,
    const char *name, rpc_object_t args, void *arg);

rpc_server_t rpc_server_create(const char *uri, rpc_context_t context);
void rpc_server_broadcast_event(rpc_server_t server, const char *name,
    rpc_object_t args);
void rpc_server_set_event_handler(rpc_server_event_handler_t handler);
void rpc_server_set_event_handler_f(rpc_server_event_handler_f handler);
int rpc_server_start(rpc_server_t server, bool background);
int rpc_server_stop(rpc_server_t server);
int rpc_server_close(rpc_server_t server);

#ifdef __cplusplus
}
#endif

#endif //LIBRPC_SERVER_H
