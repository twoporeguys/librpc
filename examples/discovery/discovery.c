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
#include <rpc/object.h>
#include <rpc/client.h>

int
main(int argc __attribute__((unused)), const char *argv[] __attribute__((unused)))
{
	rpc_client_t client;
	rpc_connection_t conn;
	rpc_object_t result;
	rpc_call_t call;

	client = rpc_client_create("tcp://127.0.0.1:5000", 0);
	conn = rpc_client_get_connection(client);

	call = rpc_connection_call(conn, "discovery.get_methods", NULL, NULL);
	rpc_call_wait(call);

	while (rpc_call_status(call) == RPC_CALL_MORE_AVAILABLE) {
		result = rpc_call_result(call);
		printf("%s (%s)\n", rpc_dictionary_get_string(result, "name"),
		    rpc_dictionary_get_string(result, "description"));

		rpc_call_continue(call, true);
	}

	rpc_call_free(call);
	rpc_client_close(client);
	return (0);
}
