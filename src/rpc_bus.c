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

#include <errno.h>
#include <glib.h>
#include "internal.h"

int
rpc_bus_ping(const char *name)
{
	struct rpc_transport *bus;

	bus = rpc_find_transport("bus");
	if (bus == NULL) {
		errno = ENXIO;
		return (-1);
	}

	if (bus->ping == NULL) {
		errno = ENXIO;
		return (-1);
	}

	return (bus->ping(name));
}

int
rpc_bus_enumerate(struct rpc_bus_node **resultp)
{
	struct rpc_transport *bus;
	struct rpc_bus_node *result;
	size_t count;

	bus = rpc_find_transport("bus");
	if (bus == NULL) {
		errno = ENXIO;
		return (-1);
	}

	if (bus->enumerate == NULL) {
		errno = ENXIO;
		return (-1);
	}

	if (bus->enumerate(&result, &count) != 0)
		return (-1);

	*resultp = result;
	return (count);
}

void
rpc_bus_free_result(struct rpc_bus_node *result)
{

	g_free(result);

}