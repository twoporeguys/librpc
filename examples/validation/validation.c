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
#include <rpc/object.h>
#include <rpc/service.h>
#include <rpc/server.h>
#include <rpc/typing.h>

#ifndef __unused
#define __unused __attribute__((unused))
#endif

static rpc_object_t animal_manager_create(void *, rpc_object_t);

static const struct rpc_if_member animal_manager_vtable[] = {
	RPC_METHOD(create, animal_manager_create),
	RPC_MEMBER_END
};

static rpc_object_t
animal_manager_create(void *cookie, rpc_object_t args)
{
	printf("Called with:\n");
	printf("%s\n", rpc_copy_description(args));

	return (rpc_string_create("OK"));
}

int
main(int argc __unused, const char *argv[] __unused)
{
	rpc_server_t srv;
	rpc_context_t ctx;
	rpc_instance_t manager;

	rpct_init();
	rpct_load_types("validation.yaml");

	ctx = rpc_context_create();
	rpc_context_set_pre_call_hook(ctx, RPC_FUNCTION(rpct_pre_call_hook));
	rpc_context_set_post_call_hook(ctx, RPC_FUNCTION(rpct_post_call_hook));

	manager = rpc_instance_new(NULL, "/animals");
	rpc_instance_register_interface(manager,
	    "com.twoporeguys.librpc.examples.AnimalManager",
	    animal_manager_vtable, NULL);

	rpc_context_register_instance(ctx, manager);
	srv = rpc_server_create("tcp://0.0.0.0:5000", ctx);
	pause();
}
