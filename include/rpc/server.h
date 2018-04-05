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

/**
 * @file server.h
 *
 * RPC server API.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * RPC server structure definition.
 */
struct rpc_server;

/**
 * RPC server pointer structure definition.
 */
typedef struct rpc_server *rpc_server_t;

/**
 * Enumerates sources of server events.
 */
typedef enum rpc_server_events
{
        RPC_CONNECTION_ARRIVED = 1,    /**< Connection to server made */
        RPC_CONNECTION_TERMINATED      /**< Connection closed or aborted */
} rpc_server_event_t;

/**
 * Definition of RPC server event handler block type.
 */
typedef void (^rpc_server_event_handler_t)(void *_Nonnull source,
    enum rpc_server_event_t type, _Nullable rpc_object_t args);

/**
 * Definition of RPC server named event handler block type.
 */
typedef 
void (^rpc_server_named_event_handler_t)(_Nonnull rpc_connection_t source,
    const char *_Nonnull name, _Nonnull rpc_object_t args);

/**
 * Converts function pointer to a @ref rpc_server_event_handler_t block type.
 */
#define	RPC_SERVER_HANDLER(_fn, _arg) 					     \
	^(void *_source, enum rpc_server_event_t type, rpc_object_t _args) { \
		_fn(_arg, _source, _type, _args);			     \
	}

/**
 * Converts function pointer to a @ref rpc_server_named_event_handler_t block 
 * type.
 */
#define	RPC_SERVER_NAMED_EVENT_HANDLER(_fn, _arg) 				\
	^(rpc_connection_t _source, const char *_name, rpc_object_t _args) {	\
		_fn(_arg, _source, _name, _args);				\
	}

/**
 * Creates a server instance listening on a given URI.
 *
 * @param uri URI to listen on
 * @param context RPC context for a server instance
 * @return Server handle
 */
_Nullable rpc_server_t rpc_server_create(const char *_Nonnull uri,
    _Nonnull rpc_context_t context);

/**
 * Starts accepting requests by the server.
 *
 * Server instance keeps all the incoming requests queued and on hold
 * until this function is called.
 *
 * @param server Server handle
 */
void rpc_server_resume(_Nonnull rpc_server_t server);

/**
 * Broadcasts an event of a given name among its subscribers.
 *
 * @param server Server handle
 * @param name Name of an event to be broadcasted
 * @param args Event arguments
 */
void rpc_server_broadcast_event(_Nonnull rpc_server_t server,
    const char *_Nullable path, const char *_Nullable interface,
    const char *_Nonnull name, _Nullable rpc_object_t args);

/**
 * Attaches a server event handler to process rpc_server_event_t events. 
 *
 * @param server server onto which to attach the handler
 * @param handler
 */
void rpc_server_set_event_handler(_Nonnull rpc_server_t server,
    _Nullable rpc_server_event_handler_t handler);

/**
 * Creates an event handler internal to a server for an event of a given name.
 *
 * @param server
 * @param name
 * @param args Arguments to pass to the event handler
 * @param handler
 */
void rpc_server_register_named_event_handler(_Nonnull rpc_server_t server,
    const char*_Nonnull name, _Nullable rpc_object_t args, 
    _Nullable rpc_server_named_event_handler_t handler);

/**
 * Closes a given RPC server.
 *
 * @param server Server handle to be closed
 * @return 0 on successful teardown
 */
int rpc_server_close(_Nonnull rpc_server_t server);

/**
 * Get the RPC server associated with a listener-side connection.
 *
 * May be used when receiving notification about the arrival/disconnect of a 
 * connection when the application has created multiple server objects.
 *
 * @param conn The connection that has status change 
 * @return The server handle or NULL if the conn is not valid
 */
_Nullable rpc_server_t  
rpc_server_get_connection_server(_Nonnull rpc_connection_t conn);

#ifdef __cplusplus
}
#endif

#endif //LIBRPC_SERVER_H
