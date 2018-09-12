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

/**
* @example pack-unpack.c
*
* An example that shows how to use @ref rpc_object_pack and
* @ref rpc_object_unpack APIs.
*/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <rpc/object.h>
#include <rpc/client.h>
#include <rpc/server.h>
#include <rpc/typing.h>
#include <rpc/service.h>

int
main(int argc, const char *argv[])
{
	rpc_context_t ctx;
	rpc_server_t server;
	rpc_client_t client;
	rpc_connection_t conn;
	rpc_object_t result;
	const char *keys[] = {"key"};
	const rpc_object_t values[] = {rpc_int64_create(11234)};

	(void)argc;
	(void)argv;

	ctx = rpc_context_create();

	rpc_context_register_block(ctx, NULL, "hello", NULL,
	    ^(void *cookie, rpc_object_t args) {
		const char *str;
		int64_t num;
		int64_t dict_num;
		int cnt;
		bool sure;
		rpc_object_t nonexistent_obj = NULL;

		(void)cookie;

		rpct_init(true);
		cnt = rpc_object_unpack(args, "[s,i,b,{nonexistent:v,key:i}]",
		    &str, &num, &sure, &nonexistent_obj, &dict_num);

		if (nonexistent_obj != NULL)
			printf("Nonexistent obj shouldn't be initialized");

		printf("unpack cnt: %i\n", cnt);

		printf("str = %s, num = %" PRId64 ", dict_num = %" PRId64
		    ", sure = %s\n", str, num, dict_num,
		    sure ? "true" : "false");

	    	return rpc_object_pack("{inline:'inline_string',s,i,uint:u,b,n,array:['inline',i,5:i,<int64>i,{s}]}",
		    "hello", "world",
		    "int", -12345L,
		    0x80808080L,
		    "true_or_false", true,
		    "nothing",
		    1L, 2L, 3L, "!", "?");
	});

	server = rpc_server_create("loopback://0", ctx);
	rpc_server_resume(server);
	if (server == NULL) {
		fprintf(stderr, "cannot create server: %s", strerror(errno));
		return (1);
	}

	client = rpc_client_create("loopback://0", 0);
	if (client == NULL) {
		fprintf(stderr, "cannot connect: %s", strerror(errno));
		return (1);
	}

	conn = rpc_client_get_connection(client);
	result = rpc_connection_call_simple(conn, "hello", "[s,i,b,v]",
	    "world", 123, true,
	    rpc_dictionary_create_ex(keys, values, 1, true));

	printf("result = %s\n", rpc_copy_description(result));

	rpc_client_close(client);
	rpc_server_close(server);
	return (0);
}
