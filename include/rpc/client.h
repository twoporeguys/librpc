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

#ifndef LIBRPC_CLIENT_H
#define LIBRPC_CLIENT_H

#include <stdbool.h>
#include <rpc/connection.h>

/**
 * @file client.h
 *
 * RPC client API.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct rpc_client;

typedef struct rpc_client *rpc_client_t;

/**
 * Creates a new, connected RPC client.
 *
 * URI parameter can take multiple forms:
 * - unix://<path> connects to an Unix domain socket
 * - tcp://<ip-address>:<port> connects using a TCP socket
 * - ws://<ip-address>:<port>/<path> connects using a WebSocket
 * - loopback://<id> connects using a local transport
 *
 * @param uri Endpoint URI
 * @param params Transport-specific parameters or NULL
 * @return Connect RPC client object
 */
rpc_client_t rpc_client_create(const char *uri, rpc_object_t params);

/**
 * Gets the connection object from a client.
 *
 * @param client Client object to get the connection from
 * @return Connection object
 */
rpc_connection_t rpc_client_get_connection(rpc_client_t client);

/**
 * Closes the connection and frees associated resources.
 *
 * @param client Client object
 */
void rpc_client_close(rpc_client_t client);

#ifdef __cplusplus
}
#endif

#endif //LIBRPC_CLIENT_H
