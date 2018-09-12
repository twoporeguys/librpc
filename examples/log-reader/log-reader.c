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
 * @example log-reader.c
 *
 * An example that shows how to read logs from a librpc-capable USB device.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <rpc/object.h>
#include <rpc/client.h>
#include <rpc/bus.h>

#define	BUFSIZE	1024

int
main(int argc, const char *argv[])
{
	rpc_client_t client;
	ssize_t nread;
	char buf[BUFSIZE];
	int fds[2];

	if (argc < 2) {
		fprintf(stderr, "Usage: log-reader <URI>\n");
		return (1);
	}

	if (pipe(fds) < 0) {
		fprintf(stderr, "pipe() failed: %s\n", strerror(errno));
		return (1);
	}

	client = rpc_client_create(argv[1], rpc_fd_create(fds[1]));
	if (client == NULL) {
		fprintf(stderr, "connect failed: %s\n", strerror(errno));
		return (1);
	}

	printf("(Connected to %s)\n", argv[1]);

	for (;;) {
		nread = read(fds[0], buf, sizeof(buf));
		if (nread == 0)
			break;

		if (nread < 0) {
			fprintf(stderr, "read() failed: %s\n", strerror(errno));
			return (1);
		}

		printf("%*s", (int)nread, buf);
		fflush(stdout);
	}

	rpc_client_close(client);
	return (0);
}
