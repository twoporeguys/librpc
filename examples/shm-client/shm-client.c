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
#include <errno.h>
#include <string.h>
#include <glib.h>
#include <rpc/object.h>
#include <rpc/client.h>

#define	BLOCK_SIZE	(1024 * 1024)

int
main(int argc, const char *argv[])
{
	rpc_client_t client;
	rpc_connection_t conn;
	rpc_object_t result;
	rpc_object_t shmem;
	void *addr;

	if (argc < 2) {
		fprintf(stderr, "Usage: shm-client <server socket URI>\n");
		return (1);
	}

	client = rpc_client_create(argv[1], 0);
	if (client == NULL) {
		fprintf(stderr, "cannot connect: %s\n", strerror(errno));
		return (1);
	}

	conn = rpc_client_get_connection(client);
	shmem = rpc_shmem_create(BLOCK_SIZE);
	addr = rpc_shmem_map(shmem);

	memset(addr, 'A', rpc_shmem_get_size(shmem));

	printf("memory before :%.*s\n", 16, addr);

	result = rpc_connection_call_simple(conn, "exchange_blob", "[v]",
	    shmem);

	printf("result = %s\n", rpc_copy_description(result));

	printf("memory after :%.*s\n", 16, addr);

	rpc_shmem_unmap(shmem, addr);
	rpc_client_close(client);
	return (0);
}
