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
#include <rpc/query.h>
#include <rpc/object.h>

int
main(int argc, const char *argv[])
{
	rpc_object_t start_object;
	rpc_object_t retval;
	rpc_query_iter_t iter;
	struct rpc_query_params params = {.offset = 1, .reverse = true};
	rpc_object_t chunk;

	(void)argc;
	(void)argv;

	start_object = rpc_object_pack("{s,i,u,b,n,[i,i,i,{s}]}",
	    "hello", "world",
	    "int", -12345L,
	    "uint", 0x80808080L,
	    "true_or_false", true,
	    "nothing",
	    "array",
	    1L, 2L, 3L, "!", "?");

	printf("start dictionary: %s\n\n", rpc_copy_description(start_object));

	printf("adding nonexistent containers with set function\n");
	rpc_query_set(start_object, "a.0.bunch.1.of.2.nonexistent.3.values", rpc_bool_create(true), true);
	retval = rpc_query_get(start_object, "a", NULL);
	printf("generated tree: %s\n\n", rpc_copy_description(retval));
	rpc_query_delete(start_object, "a");

	retval = rpc_query_get(start_object, "array.0", NULL);
	printf("array.0 (1): %s\n\n", rpc_copy_description(retval));

	retval = rpc_query_get(start_object, "array.10", rpc_int64_create(19));
	printf("array.10 (nonexistent, default: 19): %s\n\n", rpc_copy_description(retval));

	printf("%s", "Set array.0 = true\n");
	rpc_query_set(start_object, "array.0", rpc_bool_create(true), false);
	retval = rpc_query_get(start_object, "array.0", NULL);
	printf("array.0 (true): %s\n\n", rpc_copy_description(retval));

	printf("%s", "Delete array.0 (array shifts)\n");
	rpc_query_delete(start_object, "array.0");
	retval = rpc_query_get(start_object, "array.0", NULL);
	printf("array.0 (2): %s\n\n", rpc_copy_description(retval));

	printf("Contains?: array.0 (true): %s\n\n",
	    rpc_query_contains(start_object, "array.0") ? "true" : "false");
	printf("Contains?: array.10 (false): %s\n\n",
	    rpc_query_contains(start_object, "array.10") ? "true" : "false");
	printf("Contains?: test (false): %s\n\n",
	    rpc_query_contains(start_object, "test") ? "true" : "false");

	rpc_release(start_object);

	start_object = rpc_object_pack("[{u},{u},{u},{u},{u}]",
	    "value", 1,
	    "value", 2,
	    "value", 3,
	    "value", 4,
	    "value", 5);

	printf("Initial object for query (reverse, offset =1, o > 2): %s\n\n",
	    rpc_copy_description(start_object));

	iter = rpc_query_fmt(start_object, &params, "[[ssu]]",
	    "value", ">", 2);

	while (rpc_query_next(iter, &chunk)) {
		printf("%s", rpc_copy_description(chunk));
		rpc_release(chunk);
	}

	rpc_query_iter_free(iter);
	rpc_release(start_object);
}
