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
#include <getopt.h>
#include <time.h>
#include <inttypes.h>
#include <rpc/object.h>
#include <rpc/client.h>

static void timespec_diff(struct timespec *start, struct timespec *stop,
    struct timespec *result)
{
	if ((stop->tv_nsec - start->tv_nsec) < 0) {
		result->tv_sec = stop->tv_sec - start->tv_sec - 1;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
	} else {
		result->tv_sec = stop->tv_sec - start->tv_sec;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec;
	}
}

int
main(int argc, char * const argv[])
{
	struct timespec start;
	struct timespec end;
	struct timespec diff;
	double elapsed;
	rpc_object_t error;
	rpc_object_t item;
	rpc_client_t client;
	rpc_connection_t connection;
	rpc_call_t call;
	int64_t cycles = 0;
	int64_t bytes = 0;
	bool shmem = false;
	char *uri = NULL;
	int c;

	for (;;) {
		c = getopt(argc, argv, "u:mh");
		if (c == -1)
			break;

		switch (c) {
		case 'u':
			uri = strdup(optarg);
			break;

		case 'm':
			shmem = true;
			break;
		}
	}

	client = rpc_client_create(uri, NULL);
	if (client == NULL) {
		error = rpc_get_last_error();
		fprintf(stderr, "Cannot connect: %s\n",
		    rpc_error_get_message(error));
		return (EXIT_FAILURE);
	}

	connection = rpc_client_get_connection(client);
	call = rpc_connection_call(connection, "/",
	    "com.twoporeguys.librpc.Benchmark", "stream", rpc_object_pack("[i]", 10000), NULL);
	if (call == NULL) {
		error = rpc_get_last_error();
		fprintf(stderr, "Cannot start streaming: %s\n",
		    rpc_error_get_message(error));
		return (EXIT_FAILURE);
	}

	clock_gettime(CLOCK_REALTIME, &start);

next:
	rpc_call_wait(call);

	switch (rpc_call_status(call)) {
	case RPC_CALL_MORE_AVAILABLE:
		cycles++;
		item = rpc_call_result(call);

		if (shmem) {
			if (rpc_get_type(item) != RPC_TYPE_SHMEM) {
				fprintf(stderr, "Wrong response type %s\n",
				    rpc_get_type_name(rpc_get_type(item)));
				goto error;
			}
			bytes += rpc_shmem_get_size(rpc_call_result(call));
		} else {
			if (rpc_get_type(item) != RPC_TYPE_BINARY) {
				fprintf(stderr, "Wrong response type %s\n",
				    rpc_get_type_name(rpc_get_type(item)));
				goto error;
			}
			bytes += rpc_data_get_length(rpc_call_result(call));
		}

		rpc_call_continue(call, false);
		goto next;

	case RPC_CALL_DONE:
		break;

	case RPC_CALL_ERROR:
		fprintf(stderr, "Stream failed: %s\n",
		    rpc_error_get_message(rpc_call_result(call)));
		goto error;
	}

	clock_gettime(CLOCK_REALTIME, &end);
	timespec_diff(&start, &end, &diff);
	elapsed = diff.tv_sec + diff.tv_nsec / 1E9;

	printf("Received %" PRId64 " messages and %" PRId64 " bytes\n", cycles, bytes);
	printf("It took %.02f seconds\n", elapsed);
	printf("Average data rate: %.02f MB/s\n", bytes / elapsed / 1024 / 1024);
	return (EXIT_SUCCESS);

error:
	return (EXIT_FAILURE);
}