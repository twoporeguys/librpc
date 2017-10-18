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
#include <rpc/object.h>
#include <rpc/service.h>
#include <rpc/server.h>
#include <rpc/discovery.h>

#ifndef __unused
#define __unused __attribute__((unused))
#endif

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

	(void)argc;
	(void)argv;

	ctx = rpc_context_create();
	rpc_context_register_func(ctx, "hello", "Hello world function",
	    NULL, hello);

	rpc_context_register_block(ctx, "block", "Test function using blocks",
	    NULL, ^(void *cookie __unused, rpc_object_t args __unused) {
		return rpc_string_create("haha lol");
	    });


	rpc_context_register_block(ctx, "delay", "Sleeps for a long time",
	    NULL, ^(void *cookie __unused, rpc_object_t args __unused) {
		sleep(60);
		return rpc_int64_create(42);
	    });

	rpc_context_register_block(ctx, "event", "Sends a bunch of events",
	    NULL, ^(void *cookie __unused, rpc_object_t args __unused) {
		rpc_server_broadcast_event(srv, "server.hello", rpc_string_create("world"));
		rpc_server_broadcast_event(srv, "oh_noes", rpc_int64_create(-1));
		return rpc_null_create();
	    });

	rpc_discovery_register(ctx);
	srv = rpc_server_create("tcp://0.0.0.0:5000", ctx);
#ifdef _WIN32
	for (;;)
		sleep(1);
#else
	pause();
#endif
}
