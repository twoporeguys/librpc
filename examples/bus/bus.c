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
#include <rpc/bus.h>

int
main(int argc, const char *argv[])
{
	struct rpc_bus_node *nodes;
	int n, i;
	int ret;

	(void)argc;
	(void)argv;

	rpc_bus_open();

	n = rpc_bus_enumerate(&nodes);
	if (n < 0) {
		fprintf(stderr, "rpc_bus_enumerate: %s\n", strerror(errno));
		return (1);
	}

	for (i = 0; i < n; i++) {
		printf("%d: %s (%s)\n", nodes[i].rbn_address,
		    nodes[i].rbn_name, nodes[i].rbn_description);

		ret = rpc_bus_ping(nodes[i].rbn_name);
		switch (ret) {
		case 1:
			printf("    responds to ping\n");
			break;

		default:
			printf("    failed to ping, error %d [%s]\n", errno,
			    strerror(errno));
			break;
		}
	}

	rpc_bus_free_result(nodes);
	rpc_bus_close();
	return (0);
}
