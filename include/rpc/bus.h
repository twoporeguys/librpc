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

typedef enum {
    RPC_BUS_ATTACHED,
    RPC_BUS_DETACHED
} rpc_bus_event_t;

/**
 * Bus node descriptor.
 */
struct rpc_bus_node
{
    	const char *	rbn_name;
    	const char *	rbn_description;
	const char *	rbn_serial;
    	uint32_t 	rbn_address;
};

typedef void (^rpc_bus_event_handler_t)(rpc_bus_event_t event,
    struct rpc_bus_node *node);

/**
 * Converts function pointer to an rpc_bus_event_t block type.
 */
#define	RPC_BUS_EVENT_HANDLER(_fn, _arg)				\
	^(rpc_bus_event_t _event, struct rpc_bus_node *_node) {		\
                return (_fn(_arg, _node, _event));			\
        }

/**
 * Checks whether a node with specified name is reachable.
 *
 * @param name
 * @return
 */
int rpc_bus_ping(const char *name);

/**
 *
 *
 * @param result
 * @return
 */
int rpc_bus_enumerate(struct rpc_bus_node **result);

/**
 *
 *
 * @param result
 */
void rpc_bus_free_result(struct rpc_bus_node *result);

void rpc_bus_register_event_handler(rpc_bus_event_handler_t handler);
void rpc_bus_unregister_event_handler(void);


#ifdef __cplusplus
}
#endif

#endif //LIBRPC_BUS_H
