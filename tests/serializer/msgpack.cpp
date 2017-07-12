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

#include "../catch.hpp"

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <rpc/object.h>
#include "../../src/serializer/msgpack.h"
#include "../../src/serializer/json.h"
#include "../../src/internal.h"

static char json_dict_gold[] = "{"
    "\"null\":null,"
    "\"int\":-123,"
    "\"uint\":{\"$uint\":123},"
    "\"date\":{\"$date\":456},"
    "\"binary\":{\"bin\":\"MTIzNA==\"},"
    "\"fd\":{\"$fd\":1},"
    "\"double\":12.0,"
    "\"string\":\"deadbeef\","
    "\"array\":[\"woopwoop\",-1234,{\"$fd\":2}]"
    "}";

static char json_array_gold[] = "["
    "null,"
    "-123,"
    "{\"$uint\":123},"
    "{\"$date\":456},"
    "{\"bin\":\"MTIzNA==\"},"
    "{\"$fd\":1},"
    "12.0,"
    "\"deadbeef\","
    "{\"string\":\"woopwoop\",\"int\":-1234,\"fd\":{\"$fd\":2}}"
    "]";

static char json_single_gold[] = "\"asd\"";

static char json_single_ext_gold[] = "{\"$uint\":123}";

SCENARIO("MPACK_DICT_TEST", "Deserialize golden reference, serialize it again, and compare result") {
	GIVEN("MPACK object") {
		rpc_object_t object = NULL;
		rpc_object_t object_mirror = NULL;
		size_t buf_size;
		void *buf = NULL;

		object = rpc_json_deserialize((void *)json_dict_gold, strlen(json_dict_gold));

		WHEN("RPC object is serialized") {
			rpc_msgpack_serialize(object, &buf, &buf_size);

			AND_WHEN("Output buffer is deserialized again") {
				object_mirror = rpc_msgpack_deserialize(
				    buf, buf_size);

				REQUIRE(object_mirror != NULL);

				THEN("Both RPC object and its mirror retrieved from deserialized MPACK are equal") {
					REQUIRE(rpc_equal(object, object_mirror));
				}
			}
		}

		g_free(buf);
		rpc_release(object);
		rpc_release(object_mirror);
	}
}

SCENARIO("MPACK_ARRAY_TEST", "Deserialize golden reference, serialize it again, and compare result") {
	GIVEN("MPACK object") {
		rpc_object_t object = NULL;
		rpc_object_t object_mirror = NULL;
		size_t buf_size;
		void *buf = NULL;

		object = rpc_json_deserialize((void *)json_array_gold, strlen(json_array_gold));

		WHEN("RPC object is serialized") {
			rpc_msgpack_serialize(object, &buf, &buf_size);

			AND_WHEN("Output buffer is deserialized again") {
				object_mirror = rpc_msgpack_deserialize(
				    buf, buf_size);

				REQUIRE(object_mirror != NULL);

				THEN("Both RPC object and its mirror retrieved from deserialized MPACK are equal") {
					REQUIRE(rpc_equal(object, object_mirror));
				}
			}
		}

		g_free(buf);
		rpc_release(object);
		rpc_release(object_mirror);
	}
}

SCENARIO("MPACK_SINGLE_TEST", "Deserialize golden reference, serialize it again, and compare result") {
	GIVEN("MPACK object") {
		rpc_object_t object = NULL;
		rpc_object_t object_mirror = NULL;
		size_t buf_size;
		void *buf = NULL;

		object = rpc_json_deserialize((void *)json_single_gold, strlen(json_single_gold));

		WHEN("RPC object is serialized") {
			rpc_msgpack_serialize(object, &buf, &buf_size);

			AND_WHEN("Output buffer is deserialized again") {
				object_mirror = rpc_msgpack_deserialize(
				    buf, buf_size);

				REQUIRE(object_mirror != NULL);

				THEN("Both RPC object and its mirror retrieved from deserialized MPACK are equal") {
					REQUIRE(rpc_equal(object, object_mirror));
				}
			}
		}

		g_free(buf);
		rpc_release(object);
		rpc_release(object_mirror);
	}
}

SCENARIO("MPACK_SINGLE_EXT_TEST", "Deserialize golden reference, serialize it again, and compare result") {
	GIVEN("MPACK object") {
		rpc_object_t object = NULL;
		rpc_object_t object_mirror = NULL;
		size_t buf_size;
		void *buf = NULL;

		object = rpc_json_deserialize((void *)json_single_ext_gold, strlen(json_single_ext_gold));

		WHEN("RPC object is serialized") {
			rpc_msgpack_serialize(object, &buf, &buf_size);

			AND_WHEN("Output buffer is deserialized again") {
				object_mirror = rpc_msgpack_deserialize(
				    buf, buf_size);

				REQUIRE(object_mirror != NULL);

				THEN("Both RPC object and its mirror retrieved from deserialized MPACK are equal") {
					REQUIRE(rpc_equal(object, object_mirror));
				}
			}
		}

		g_free(buf);
		rpc_release(object);
		rpc_release(object_mirror);
	}
}

#if defined(__linux__)
SCENARIO("MPACK_SHMEM_TEST", "Serialize and deserialize an RPC object representing a chunk of shared memory") {
	GIVEN("Chunk of shared memory") {
		rpc_object_t shmem = rpc_shmem_create(1024);
		rpc_object_t parent = rpc_dictionary_create();
		rpc_object_t mirror = NULL;
		size_t buf_size;
		void *buf = NULL;

		rpc_dictionary_steal_value(parent, "mem", shmem);

		WHEN("RPC object is serialized and deserialized again") {
			rpc_msgpack_serialize(parent, &buf, &buf_size);
			mirror = rpc_msgpack_deserialize(buf, buf_size);

			THEN("Both source and result are the same") {
				REQUIRE(rpc_equal(parent, mirror));
			}
		}

		rpc_release(parent);
		rpc_release(mirror);
		g_free(buf);
	}
}
#endif
