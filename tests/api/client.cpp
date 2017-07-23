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

#include "../catch.hpp"
#include <rpc/object.h>
#include <rpc/service.h>
#include <rpc/server.h>
#include <rpc/client.h>
#include <string.h>
#include "../src/internal.h"

static rpc_object_t
hello(void *cookie __unused, rpc_object_t args)
{

	return rpc_string_create_with_format("hello %s!",
	    rpc_array_get_string(args, 0));
}

SCENARIO("RPC_CLIENT", "Create a simple RPC server and connect to it") {
	GIVEN("A simple RPC server") {
		rpc_client_t client = NULL;
		rpc_connection_t conn;
		rpc_object_t result = NULL;
		rpc_context_t ctx = NULL;
		__block rpc_server_t srv = NULL;

		ctx = rpc_context_create();
		rpc_context_register_func(ctx, "hello", "Hello world function",
		    NULL, hello);

		srv = rpc_server_create("tcp://0.0.0.0:5000", ctx);

		client = rpc_client_create("tcp://127.0.0.1:5000", 0);
		conn = rpc_client_get_connection(client);

		WHEN("Client is connected") {
			THEN("Connection has been successfully established") {
				REQUIRE(client != NULL);
			}

			THEN("Connection is not NULL") {
				REQUIRE(conn != NULL);
			}

			AND_WHEN("Client calls an RPC method") {
				result = rpc_connection_call_sync(conn, "hello",
				    rpc_string_create("world"), NULL);

				THEN("Result is not NULL") {
					REQUIRE(result != NULL);
				}

				THEN("Results says: hello world!") {
					REQUIRE(!strcmp(
					    rpc_string_get_string_ptr(result),
					    "hello world!"));
				}

				rpc_release(result);
			}

			AND_WHEN("Client calls an RPC method again") {
				result = rpc_connection_call_sync(conn, "hello",
				    rpc_string_create("world"), NULL);

				THEN("Result is not NULL") {
					REQUIRE(result != NULL);
				}

				THEN("Results says: hello world!") {
					REQUIRE(!strcmp(
					    rpc_string_get_string_ptr(result),
					    "hello world!"));
				}

				rpc_release(result);
			}
		}

		if (client != NULL)
			rpc_client_close(client);

		if (srv != NULL)
			rpc_server_close(srv);

		if (ctx != NULL)
			rpc_context_free(ctx);
	}
}
