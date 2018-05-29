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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <rpc/object.h>
#include <rpc/service.h>
#include <rpc/server.h>

static char *uri = NULL;
static size_t msgsize = 4096;
static bool shmem = false;

static rpc_object_t benchmark_stream(void *, rpc_object_t);
void usage(const char *);
int main(int, char * const []);

static const struct rpc_if_member benchmark_vtable[] = {
	RPC_METHOD(stream, benchmark_stream),
	RPC_MEMBER_END
};

static rpc_object_t
benchmark_stream(void *cookie, rpc_object_t args)
{
	rpc_object_t data;
	int64_t cycles;
	void *buffer;

	if (rpc_object_unpack(args, "[i]", &cycles) < 1) {
		rpc_function_error(cookie, EINVAL, "Invalid arguments passed");
		return (NULL);
	}

	if (shmem) {
		data = rpc_shmem_create(msgsize);
		buffer = rpc_shmem_map(data);
	} else {
		buffer = malloc(msgsize);
		data = rpc_data_create(buffer, msgsize,
		    RPC_BINARY_DESTRUCTOR(free));
	}

	memset(buffer, 0x55, msgsize);

	while (cycles--) {
		if (rpc_function_yield(cookie, rpc_retain(data)) < 0)
			break;
	}

	return (NULL);
}

void
usage(const char *argv0)
{

	fprintf(stderr, "Usage: %s -u URI [-s MSGSIZE] [-m]\n", argv0);
	fprintf(stderr, "       %s -h\n", argv0);
}

int
main(int argc, char * const argv[])
{
	rpc_context_t context;
	rpc_server_t server;
	int c;

	for (;;) {
		c = getopt(argc, argv, "u:s:mh");
		if (c == -1)
			break;

		switch (c) {
		case 'u':
			uri = strdup(optarg);
			break;

		case 's':
			msgsize = (size_t)strtol(optarg, NULL, 10);
			break;

		case 'm':
			shmem = true;
			break;

		case 'h':
			usage(argv[0]);
			return (EXIT_SUCCESS);
		}
	}

	if (uri == NULL) {
		fprintf(stderr, "Error: URI not specified\n");
		usage(argv[0]);
		return (EXIT_SUCCESS);
	}

	context = rpc_context_create();
	rpc_instance_register_interface(rpc_context_get_root(context),
	    "com.twoporeguys.librpc.Benchmark", benchmark_vtable, NULL);

	server = rpc_server_create(uri, context);
	rpc_server_resume(server);

	printf("Listening on %s\n", uri);
	printf("Using %zu message size\n", msgsize);

	if (shmem)
		printf("Using shared memory\n");

	pause();

	return (EXIT_SUCCESS);
}
