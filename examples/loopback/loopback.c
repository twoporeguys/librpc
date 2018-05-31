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
#include <errno.h>
#include <string.h>
#include <rpc/object.h>
#include <rpc/client.h>
#include <rpc/server.h>
#include <rpc/service.h>

int
main(int argc, const char *argv[])
{
	rpc_context_t ctx;
	rpc_server_t server;
	rpc_client_t client;
	rpc_connection_t conn;
	rpc_object_t result;

	ctx = rpc_context_create();

	rpc_context_register_block(ctx, NULL, "hello", NULL,
	    ^(void *cookie, rpc_object_t args) {
	    	return rpc_string_create("world");
	});

	server = rpc_server_create("loopback://0", ctx);
	if (server == NULL) {
		fprintf(stderr, "cannot create server: %s", strerror(errno));
		return (1);
	}

	client = rpc_client_create("loopback://0", 0);
	if (client == NULL) {
		fprintf(stderr, "cannot connect: %s", strerror(errno));
		return (1);
	}

	rpc_server_resume(server);

	conn = rpc_client_get_connection(client);
	result = rpc_connection_call_simple(conn, "hello", "[s]", "world");
	printf("result = %s\n", rpc_string_get_string_ptr(result));

	rpc_client_close(client);
	rpc_server_close(server);
	return (0);
}
