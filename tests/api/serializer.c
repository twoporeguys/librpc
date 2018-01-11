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

#include "../tests.h"
#include "../../src/linker_set.h"
#include <glib.h>
#include <string.h>
#include <rpc/object.h>
#include <rpc/serializer.h>


typedef struct {
	rpc_object_t object;
	const char *type;
} serializer_fixture;

static void
serializer_test(serializer_fixture *fixture, gconstpointer user_data)
{
	rpc_object_t object = fixture->object;
	const char *type = fixture->type;
	rpc_object_t object_mirror = NULL;
	size_t buf_size;
	void *buf = NULL;

	g_assert(rpc_serializer_dump(type, object, &buf, &buf_size) == 0);
	object_mirror = rpc_serializer_load(type, buf, buf_size);
	g_assert_nonnull(object_mirror);
	g_assert_true(rpc_equal(object, object_mirror));

	g_test_queue_free(buf);
	g_test_queue_destroy((GDestroyNotify)rpc_release_impl, object_mirror);
}

static void
serializer_test_dict_set_up(serializer_fixture *fixture,
    gconstpointer user_data)
{
	void *data;
	size_t size = (size_t)g_test_rand_int_range(1024, 1024*1024);
	uint64_t uint = (uint64_t)g_test_rand_int();

	data = g_malloc0(size);
	memset(data, g_test_rand_int_range(0, 255), size);

	fixture->object = rpc_object_pack("{n,b,i,u,D,B,f,d,s,[s,i,f]}",
	    "null",
	    "bool", true,
	    "int", (int64_t)g_test_rand_int(),
	    "uint", uint,
	    "date", (int64_t)g_test_rand_int_range(0, 0xffff),
	    "binary", data, size, RPC_BINARY_DESTRUCTOR(g_free),
	    "fd", 1,
	    "double", g_test_rand_double(),
	    "string", "deadbeef",
	    "array",
	    "woopwoop",
	    (int64_t)g_test_rand_int(),
	    1);

	fixture->type = user_data;
}

static void
serializer_test_array_set_up(serializer_fixture *fixture,
    gconstpointer user_data)
{
	void *data;
	size_t size = (size_t)g_test_rand_int_range(1024, 1024*1024);

	data = g_malloc0(size);
	memset(data, g_test_rand_int_range(0, 255), size);

	fixture->object = rpc_object_pack("[n,b,i,u,D,B,f,d,s,{s,i,f}]",
	    true,
	    (int64_t)g_test_rand_int(),
	    (uint64_t)g_test_rand_int(),
	    (int64_t)g_test_rand_int_range(0, 0xffff),
	    data, size, RPC_BINARY_DESTRUCTOR(g_free),
	    1,
	    g_test_rand_double(),
	    "deadbeef",
	    "string", "woopwoop",
	    "int", (int64_t)g_test_rand_int(),
	    "fd", 1);

	fixture->type = user_data;
}

static void
serializer_test_single_set_up(serializer_fixture *fixture,
    gconstpointer user_data)
{

	fixture->object = rpc_object_pack("u", (uint64_t)g_test_rand_int());

	fixture->type = user_data;
}

#if defined(__linux__)
static void
serializer_test_shmem_set_up(serializer_fixture *fixture,
    gconstpointer user_data)
{
	size_t size = (size_t)g_test_rand_int_range(1024, 1024*1024);

	fixture->object = rpc_shmem_create(size);

	fixture->type = user_data;
}
#endif

static void
serializer_test_tear_down(serializer_fixture *fixture,
    gconstpointer user_data)
{

	rpc_release(fixture->object);
}

static void
serializer_test_register()
{

	g_test_add("/serializer/json/dict", serializer_fixture, "json",
	    serializer_test_dict_set_up, serializer_test,
	    serializer_test_tear_down);
	g_test_add("/serializer/json/array", serializer_fixture, "json",
	    serializer_test_array_set_up, serializer_test,
	    serializer_test_tear_down);
	g_test_add("/serializer/json/single", serializer_fixture, "json",
	    serializer_test_single_set_up, serializer_test,
	    serializer_test_tear_down);

	g_test_add("/serializer/msgpack/dict", serializer_fixture, "msgpack",
	    serializer_test_dict_set_up, serializer_test,
	    serializer_test_tear_down);
	g_test_add("/serializer/msgpack/array", serializer_fixture, "msgpack",
	    serializer_test_array_set_up, serializer_test,
	    serializer_test_tear_down);
	g_test_add("/serializer/msgpack/single", serializer_fixture, "msgpack",
	    serializer_test_single_set_up, serializer_test,
	    serializer_test_tear_down);

	g_test_add("/serializer/yaml/dict", serializer_fixture, "yaml",
	    serializer_test_dict_set_up, serializer_test,
	    serializer_test_tear_down);
	g_test_add("/serializer/yaml/array", serializer_fixture, "yaml",
	    serializer_test_array_set_up, serializer_test,
	    serializer_test_tear_down);
	g_test_add("/serializer/yaml/single", serializer_fixture, "yaml",
	    serializer_test_single_set_up, serializer_test,
	    serializer_test_tear_down);


	#if defined(__linux__)
	g_test_add("/serializer/json/shmem", serializer_fixture, "json",
	    serializer_test_shmem_set_up, serializer_test,
	    serializer_test_tear_down);
	g_test_add("/serializer/msgpack/shmem", serializer_fixture, "msgpack",
	    serializer_test_shmem_set_up, serializer_test,
	    serializer_test_tear_down);
	g_test_add("/serializer/yaml/shmem", serializer_fixture, "yaml",
	    serializer_test_shmem_set_up, serializer_test,
	    serializer_test_tear_down);
	#endif
}

static struct librpc_test serializer = {
    .name = "serializer",
    .register_f = &serializer_test_register
};

DECLARE_TEST(serializer);