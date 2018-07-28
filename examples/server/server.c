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

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <rpc/object.h>
#include <rpc/service.h>
#include <rpc/server.h>

#ifndef __unused
#define __unused __attribute__((unused))
#endif

static void
server_event(void *arg __unused, rpc_connection_t conn,
    rpc_server_event_t event)
{
	const char *addr;

	addr = rpc_connection_get_remote_address(conn);

	if (event == RPC_SERVER_CLIENT_CONNECT)
		printf("client %s connected\n", addr);

	if (event == RPC_SERVER_CLIENT_DISCONNECT)
		printf("client %s disconnected\n", addr);
}

static rpc_object_t
hello(void *cookie __unused, rpc_object_t args)
{
	(void)cookie;

	return rpc_string_create_with_format("hello %s!",
	    rpc_array_get_string(args, 0));
}

int
main(int argc, const char *argv[])
{
	rpc_context_t ctx;
	__block rpc_server_t srv;
        __block GRand *rand = g_rand_new();
        __block gint setcnt = g_rand_int_range(rand, 50, 500);
        __block char *strg = g_malloc(27);

	(void)argc;
	(void)argv;

	ctx = rpc_context_create();
	rpc_context_register_func(ctx, NULL, "hello",
	    NULL, hello);

	rpc_context_register_block(ctx, NULL, "block",
	    NULL, ^(void *cookie __unused, rpc_object_t args __unused) {
		return (rpc_string_create("haha lol"));
	    });

	rpc_context_register_block(ctx, NULL, "delay",
	    NULL, ^(void *cookie __unused, rpc_object_t args __unused) {
		sleep(60);
		return (rpc_int64_create(42));
	    });

	rpc_context_register_block(ctx, NULL, "event",
	    NULL, ^(void *cookie __unused, rpc_object_t args __unused) {
		rpc_server_broadcast_event(srv, NULL, NULL, "server.hello",
		    rpc_string_create("world"));
		rpc_server_broadcast_event(srv, NULL, NULL, "oh_noes",
		    rpc_int64_create(-1));
		return (rpc_null_create());
	    });

        strcpy(strg, "abcdefghijklmnopqrstuvwxyz");
        rpc_context_register_block(ctx, NULL, "stream",
            NULL, ^rpc_object_t (void *cookie, rpc_object_t args __unused) {
                int cnt = 0;
                gint i;
                rpc_object_t res;

                while (cnt < setcnt) {
                        cnt++;

                        i = g_rand_int_range (rand, 0, 26);

                        res = rpc_object_pack("[s, i, i]",
                            strg + i, 26-i, cnt);

                        fprintf(stderr, "returning %s,  %d letters, %d of %d\n",
                            rpc_array_get_string(res, 0), 26-i, cnt, setcnt);

                        if (rpc_function_yield(cookie, res) != 0) {
                                fprintf(stderr, "yield failed\n");
                                rpc_function_end(cookie);
                                return (rpc_null_create());
                        }
                }
                rpc_function_end(cookie);
		return (rpc_null_create());
        });

	srv = rpc_server_create("tcp://0.0.0.0:5000", ctx);
	rpc_server_set_event_handler(srv, RPC_SERVER_HANDLER(server_event, NULL));
	rpc_server_resume(srv);
	sleep(30);
	rpc_server_close(srv);
	return(0);
}
