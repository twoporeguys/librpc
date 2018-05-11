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
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <errno.h>
#include <string.h>
#include <rpc/object.h>
#include <rpc/client.h>

int
main(int argc, const char *argv[])
{
	rpc_client_t client;
	rpc_connection_t conn;
	rpc_object_t result;
	rpc_call_t call;
	const char *buf;

	int64_t len;
	int i;
	int64_t num;
	int cnt = 0;

	(void)argc;
	(void)argv;

	//client = rpc_client_create("tcp://127.0.0.1:5000", 0);
	//client = rpc_client_create("unix://../test.sock", 0);

	client = rpc_client_create("ws://127.0.0.1:5005/ws", 0);
	if (client == NULL) {
		fprintf(stderr, "cannot connect: %s", strerror(errno));
		return (1);
	}

	conn = rpc_client_get_connection(client);
	result = rpc_connection_call_simple(conn, "hello", "[s]", "world");
	printf("result = %s\n", rpc_string_get_string_ptr(result));
	rpc_release(result);

	result = rpc_connection_call_simple(conn, "hello", "[s]", "world");
	printf("result = %s\n", rpc_string_get_string_ptr(result));
	rpc_release(result);

        call = rpc_connection_call(conn, NULL, NULL, "stream", rpc_array_create(), NULL);
        if (call == NULL) {
                fprintf(stderr, "Stream call failed\n");
                rpc_client_close(client);
                return (1);
        }
        for (;;) {
                rpc_call_wait(call);

                switch (rpc_call_status(call)) {
                        case RPC_CALL_MORE_AVAILABLE:
				result = rpc_call_result(call);
                                i = rpc_object_unpack(result,
                                    "[s, i, i]", &buf, &len, &num);
                                cnt++;
				fprintf(stderr, "frag = %s, len = %lld, num = %lld,"
				    "cnt = %d\n", buf, len, num, cnt);

				g_assert(len == (int)strlen(buf));
                                rpc_call_continue(call, false);
                                break;

                        case RPC_CALL_DONE:
                        case RPC_CALL_ENDED:
				fprintf(stderr, "ENDED at %d\n", cnt);
                                goto done;

                        case RPC_CALL_ERROR:
				fprintf(stderr, "ERRED out\n");
                                goto done;

                        default:
				break;
                }
        }

done:
        fprintf(stderr, "CLOSING client conn %p, cnt = %d\n", conn, cnt);

	rpc_client_close(client);
	return (0);
}
