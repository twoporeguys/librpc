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
* @example fd-server.c
*
* An example that shows how to send a file descriptor to a client
* over librpc connection.
*/

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <rpc/object.h>
#include <rpc/service.h>
#include <rpc/server.h>

static rpc_object_t
write_to_pipe(void *cookie, rpc_object_t args)
{
	int fd;

	if (rpc_object_unpack(args, "[f]", &fd) < 1) {
		rpc_function_error(cookie, EINVAL, "Invalid arguments passed");
		return (NULL);
	}

	printf("Received fd %d\n", fd);

	dprintf(fd, "Hello there\n");
	dprintf(fd, "I am writing to the pipe\n");
	sleep(1);
	dprintf(fd, "And sometimes sleeping\n");
	dprintf(fd, "Bye.\n");
	close(fd);

	return (NULL);
}

int
main(int argc, const char *argv[])
{
	rpc_context_t ctx;
	rpc_server_t srv;

	(void)argc;
	(void)argv;

	ctx = rpc_context_create();
	rpc_context_register_func(ctx, NULL, "write_to_pipe", NULL,
	    write_to_pipe);
	srv = rpc_server_create("unix:///tmp/server.sock", ctx);
	rpc_server_resume(srv);
	pause();
}
