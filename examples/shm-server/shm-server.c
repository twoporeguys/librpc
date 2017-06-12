/*
 * Copyright 2017 Two Pore Guys, Inc.
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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <rpc/object.h>
#include <rpc/service.h>
#include <rpc/server.h>
#include <rpc/discovery.h>
#include "../../src/internal.h"

static rpc_object_t
exchange_blob(void *cookie, rpc_object_t args)
{
	rpc_object_t shmem;
	void *addr;

//	if (rpc_object_unpack(args, "[h]", shmem) != 0) {
//		rpc_function_error(cookie, EINVAL, "Invalid arguments passed");
//		return (NULL);
//	}

	shmem = rpc_array_get_value(args, 0);

	printf("Received %zu bytes long shared memory block\n",
	    rpc_shmem_get_size(shmem));

	addr = rpc_shmem_map(shmem);

	memset(addr, 'B', rpc_shmem_get_size(shmem));

	rpc_shmem_unmap(shmem, addr);
	return (shmem);
}

int
main(int argc, const char *argv[])
{
	rpc_context_t ctx;
	rpc_server_t srv;

	(void)argc;
	(void)argv;

	ctx = rpc_context_create();
	rpc_context_register_func(ctx, "exchange_blob", "Exchanges binary blob",
	    NULL, &exchange_blob);

	rpc_discovery_register(ctx);
	srv = rpc_server_create("unix:///tmp/server.sock", ctx);
	pause();
}
