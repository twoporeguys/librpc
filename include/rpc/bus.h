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

#ifndef LIBRPC_BUS_H
#define LIBRPC_BUS_H

#include <stdint.h>

/**
 * @file bus.h
 *
 * Bus transport API
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Bus hot-plug event type.
 */
typedef enum {
	RPC_BUS_ATTACHED,	/**< Device attached to the system */
	RPC_BUS_DETACHED	/**< Device detached from the system */
} rpc_bus_event_t;

/**
 * Bus node descriptor.
 *
 * Node address (@ref rbn_address) and serial number (@ref rbn_serial) fields
 * are guaranteed to be unique.
 */
struct rpc_bus_node
{
	const char *_Nullable	rbn_name;	/**< Node name */
	const char *_Nullable	rbn_description;/**< Description */
	const char *_Nullable	rbn_serial;	/**< Device serial number */
	uint32_t 		rbn_address;	/**< Address on the bus */
};

/**
 * Hotplug event handler callback block type.
 */
typedef void (^rpc_bus_event_handler_t)(rpc_bus_event_t event,
    struct rpc_bus_node *_Nonnull node);

/**
 * Converts function pointer to an @ref rpc_bus_event_t block type.
 */
#define	RPC_BUS_EVENT_HANDLER(_fn, _arg)				\
    ^(rpc_bus_event_t _event, struct rpc_bus_node *_node) {		\
            _fn(_arg, _event, _node);					\
    }

/**
 * Opens the librpc bus connection.
 *
 * @return 0 on success, -1 on error
 */
int rpc_bus_open(void);

/**
 * Closes the librpc bus connection.
 *
 * @return 0 on success, -1 on error
 */
int rpc_bus_close(void);

/**
 * Checks whether a node with specified serial is reachable.
 *
 * @param serial Node serial number
 * @return 0 if reachable, -1 otherwise
 */
int rpc_bus_ping(const char *_Nonnull serial);

/**
 * Enumerates connected devices on the RPC bus.
 *
 * @param result Array of @ref rpc_bus_node elements
 * @return 0 on success, -1 on error
 */
int rpc_bus_enumerate(struct rpc_bus_node *_Nullable *_Nonnull result);

/**
 * Frees struct rpc_bus_node array obtained in @ref rpc_bus_enumerate call.
 *
 * @param result Array of struct rpc_bus_node elements
 */
void rpc_bus_free_result(struct rpc_bus_node *_Nonnull result);

/**
 * Configures an event handler block to be called whenever a bus
 * event occurs.
 *
 * @param handler Bus event handler
 */
void rpc_bus_register_event_handler(_Nonnull rpc_bus_event_handler_t handler);

/**
 * Unsets the previously set event handler. If there was no handler previously
 * configured, does nothing.
 */
void rpc_bus_unregister_event_handler(void);


#ifdef __cplusplus
}
#endif

#endif /* LIBRPC_BUS_H */
