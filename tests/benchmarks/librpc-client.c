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

static void timespec_diff(struct timespec *, struct timespec *,
    struct timespec *);
void usage(const char *);
int main(int, char * const[]);

static void
timespec_diff(struct timespec *start, struct timespec *stop,
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

void
usage(const char *argv0)
{

	fprintf(stderr, "Usage: %s -u URI [-c CYCLES] [-m]\n", argv0);
	fprintf(stderr, "       %s -h\n", argv0);
}

int
main(int argc, char * const argv[])
{
	struct timespec start;
	struct timespec end;
	struct timespec lat_start;
	struct timespec lat_end;
	struct timespec diff;
	double elapsed;
	double lat_sum = 0;
	rpc_object_t error;
	rpc_object_t item;
	rpc_client_t client;
	rpc_connection_t connection;
	rpc_call_t call;
	int64_t cycles = 0;
	int64_t bytes = 0;
	int64_t ncycles = 1000;
	bool shmem = false;
	bool quiet = false;
	char *uri = NULL;
	int c;

	for (;;) {
		c = getopt(argc, argv, "u:c:mhq");
		if (c == -1)
			break;

		switch (c) {
		case 'u':
			uri = strdup(optarg);
			break;

		case 'c':
			ncycles = strtoll(optarg, NULL, 10);
			break;

		case 'm':
			shmem = true;
			break;

		case 'q':
			quiet = true;
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

	client = rpc_client_create(uri, NULL);
	if (client == NULL) {
		error = rpc_get_last_error();
		fprintf(stderr, "Cannot connect: %s\n",
		    rpc_error_get_message(error));
		return (EXIT_FAILURE);
	}

	connection = rpc_client_get_connection(client);
	call = rpc_connection_call(connection, "/",
	    "com.twoporeguys.librpc.Benchmark", "stream",
	    rpc_object_pack("[i]", ncycles), NULL);
	if (call == NULL) {
		error = rpc_get_last_error();
		fprintf(stderr, "Cannot start streaming: %s\n",
		    rpc_error_get_message(error));
		return (EXIT_FAILURE);
	}

	clock_gettime(CLOCK_REALTIME, &start);
	lat_start = start;

	rpc_call_set_prefetch(call, 128);
	rpc_call_wait(call);

next:
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

		clock_gettime(CLOCK_REALTIME, &lat_end);
		timespec_diff(&lat_start, &lat_end, &diff);
		lat_sum += diff.tv_sec + diff.tv_nsec / 1E9;

		clock_gettime(CLOCK_REALTIME, &lat_start);
		rpc_call_continue(call, true);
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

	if (quiet) {
		printf("msgs=%" PRId64 " bytes=%" PRId64 " bps=%f pps=%f lat=%f\n",
		    cycles, bytes, bytes / elapsed, cycles / elapsed,
		    lat_sum / cycles);
	} else {
		printf("Received %" PRId64 " messages and %" PRId64 " bytes\n", cycles, bytes);
		printf("It took %.04f seconds\n", elapsed);
		printf("Average data rate: %.04f MB/s\n", bytes / elapsed / 1024 / 1024);
		printf("Average packet rate: %.04f packets/s\n", cycles / elapsed);
		printf("Average latency: %.08fs\n", lat_sum / cycles);
	}

	return (EXIT_SUCCESS);

error:
	return (EXIT_FAILURE);
}
