/*
SCENARIO("RPC_DICTIONARY_OBJECT", "Create a DICTIONARY RPC object and perform basic operations on it") {
	GIVEN("DICTIONARY object") {
		rpc_object_t object;
		rpc_object_t different_object;
		rpc_object_t copy;
		int data = 0xFF00FF00;
		size_t data_len;
		size_t *data_len_ptr;
		int fds[2];
		int dup_fd = 0;
		struct stat stat1, stat2;

		data_len_ptr = &data_len;
		pipe(fds);

		object = rpc_dictionary_create();
		different_object = rpc_dictionary_create();

		THEN("Empty object has no entries") {
			REQUIRE(rpc_dictionary_get_count(object) == 0);

			REQUIRE(!rpc_dictionary_get_bool(object, "bool_val"));
			REQUIRE(rpc_dictionary_get_int64(object, "int_val") == 0);
			REQUIRE(rpc_dictionary_get_uint64(object, "uint_val") == 0);
			REQUIRE(rpc_dictionary_get_double(object, "double_val") == 0);
			REQUIRE(rpc_dictionary_get_date(object, "date_val") == 0);
			REQUIRE(rpc_dictionary_get_data(object, "data_val", data_len_ptr) == NULL);
			REQUIRE(rpc_dictionary_get_string(object, "string_val") == NULL);
			REQUIRE(rpc_dictionary_get_fd(object, "fd_val") == 0);
			REQUIRE(rpc_dictionary_dup_fd(object, "fd_val") == 0);

			REQUIRE(rpc_dictionary_get_value(object, "nonexistent_key") == NULL);
		}

		THEN("Type is DICTIONARY") {
			REQUIRE(rpc_get_type(object) == RPC_TYPE_DICTIONARY);
		}

		THEN("Refcount equals 1") {
			REQUIRE(object->ro_refcnt == 1);
		}

		WHEN("reference count is incremented") {
			rpc_retain(object);

			THEN("reference count equals 2"){
				REQUIRE(object->ro_refcnt == 2);
			}

			AND_WHEN("reference count is decremented") {
				rpc_release(object);

				THEN("reference count equals 1") {
					REQUIRE(object->ro_refcnt == 1);
				}

				AND_WHEN("reference count reaches 0") {
					rpc_release(object);

					THEN("RPC object pointer is NULL") {
						REQUIRE(object == NULL);
					}
				}
			}
		}

		if (object != NULL)
			rpc_release(object);

		rpc_release(different_object);

		WHEN("Dictionary tree is created") {
			object = rpc_dictionary_create();
			different_object = rpc_dictionary_create();

			rpc_dictionary_set_bool(object, "bool_val", true);
			rpc_dictionary_set_int64(object, "int_val", -1234);
			rpc_dictionary_set_uint64(object, "uint_val", 1234);
			rpc_dictionary_set_double(object, "double_val", 12.34);
			rpc_dictionary_set_date(object, "date_val", 1000);
			rpc_dictionary_set_data(object, "data_val", &data, sizeof(data));
			rpc_dictionary_set_string(object, "string_val", "test string");
			rpc_dictionary_set_fd(object, "fd_val", fds[0]);

			rpc_dictionary_set_string(different_object, "key", "value");

			THEN("Dictionary item count matches the number of inserted items") {
				REQUIRE(rpc_dictionary_get_count(object) == 8);
			}

			AND_WHEN("When one of the values is removed") {
				rpc_dictionary_remove_key(object, "int_val");

				THEN("Dictionary item count is decremented") {
					REQUIRE(rpc_dictionary_get_count(object) == 7);
				}

				THEN("Removed key does not exist in the dictionary anymore") {
					REQUIRE(!rpc_dictionary_has_key(object, "int_val"));
					REQUIRE(rpc_dictionary_get_int64(object, "int_val") == 0);
				}
			}

			AND_WHEN("One of existing items is being overwritten by NULL") {
				rpc_dictionary_set_value(object, "double_val", NULL);

				THEN("Key was removed from the dictionary") {
					REQUIRE(rpc_dictionary_get_count(object) == 7);
					REQUIRE(rpc_dictionary_get_int64(object, "double_val") == 0);
					REQUIRE(!rpc_dictionary_has_key(object, "double_val"));
				}
			}

			THEN("Extracted bool value matches initial value") {
				REQUIRE(rpc_dictionary_has_key(object, "bool_val"));
				REQUIRE(rpc_dictionary_get_bool(object, "bool_val"));
			}

			THEN("Extracted integer value matches initial value") {
				REQUIRE(rpc_dictionary_has_key(object, "int_val"));
				REQUIRE(rpc_dictionary_get_int64(object, "int_val") == -1234);
			}

			THEN("Extracted unsigned integer value matches initial value") {
				REQUIRE(rpc_dictionary_has_key(object, "uint_val"));
				REQUIRE(rpc_dictionary_get_uint64(object, "uint_val") == 1234);
			}

			THEN("Extracted double value matches initial value") {
				REQUIRE(rpc_dictionary_has_key(object, "double_val"));
				REQUIRE(rpc_dictionary_get_double(object, "double_val") == 12.34);
			}

			THEN("Extracted date value matches initial value") {
				REQUIRE(rpc_dictionary_has_key(object, "date_val"));
				REQUIRE(rpc_dictionary_get_date(object, "date_val") == 1000);
			}

			THEN("Extracted data pointer matches initial data pointer") {
				REQUIRE(rpc_dictionary_has_key(object, "data_val"));
				REQUIRE(rpc_dictionary_get_data(object, "data_val", data_len_ptr) == &data);
			}

			THEN("Extracted string value matches initial value") {
				REQUIRE(rpc_dictionary_has_key(object, "string_val"));
				REQUIRE(g_strcmp0(rpc_dictionary_get_string(object, "string_val"), "test string") == 0);
			}

			THEN("Extracted fd value matches initial value") {
				REQUIRE(rpc_dictionary_has_key(object, "fd_val"));
				REQUIRE(rpc_dictionary_get_fd(object, "fd_val") == fds[0]);
			}

			THEN("Duplicated fd has different value, but references the same file") {
				dup_fd = rpc_dictionary_dup_fd(object, "fd_val");

				REQUIRE(rpc_dictionary_get_fd(object, "fd_val") != dup_fd);
				REQUIRE(fstat(rpc_dictionary_get_fd(object, "fd_val"), &stat1) >= 0);
				REQUIRE(fstat(dup_fd, &stat2) >= 0);
				REQUIRE(stat1.st_dev == stat2.st_dev);
				REQUIRE(stat1.st_ino == stat2.st_ino);
			}

			WHEN("Object's copy is created") {
				copy = rpc_copy(object);

				THEN("Source and copy are equal") {
					REQUIRE(rpc_equal(object, copy));
				}

				AND_THEN("Object is different from object initialized with different value") {
					REQUIRE(!rpc_equal(object, different_object));
				}

				rpc_release(copy);
			}
		}


		WHEN("Object is created with initial values and steals them") {
			const char *names[] = {"string", "int"};
			const rpc_object_t values[] = {
			    rpc_string_create("test string"),
			    rpc_int64_create(64)
			};

			rpc_object_t object_to_steal;
			rpc_object_t object_to_set;

			object = rpc_dictionary_create_ex(names, values, 2, true);
			different_object = rpc_dictionary_create();

			rpc_dictionary_set_string(different_object, "key", "another test string");

			THEN("Object contains both items inserted during initialization") {
				REQUIRE(rpc_dictionary_get_count(object) == 2);

				REQUIRE(g_strcmp0(rpc_dictionary_get_string(object, "string"), "test string") == 0);
				REQUIRE(rpc_dictionary_get_int64(object, "int") == 64);
			}

			AND_WHEN("Value is stolen") {
				object_to_steal = rpc_array_create();
				rpc_dictionary_steal_value(object, "stolen", object_to_steal);

				THEN("Entry in dictionary references the original value") {
					REQUIRE(rpc_dictionary_has_key(object, "stolen"));
					REQUIRE(rpc_dictionary_get_value(object, "stolen") == object_to_steal);

					AND_THEN("Reference count of stolen object remains 1") {
						REQUIRE(rpc_dictionary_get_value(object, "stolen")->ro_refcnt == 1);
					}
				}
			}

			AND_WHEN("Value is set") {
				object_to_set = rpc_array_create();
				rpc_dictionary_set_value(object, "set", object_to_set);

				THEN("Entry in dictionary references the original value") {
					REQUIRE(rpc_dictionary_has_key(object, "set"));
					REQUIRE(rpc_dictionary_get_value(object, "set") == object_to_set);

					AND_THEN("Reference count of the set value was incremented") {
						REQUIRE(rpc_dictionary_get_value(object, "set")->ro_refcnt == 2);
					}
				}

				rpc_release(object_to_set);
			}
		}

		WHEN("Object is created with initial values") {
			const char *names[] = {"string", "int"};
			const rpc_object_t values[] = {
			    rpc_string_create("test string"),
			    rpc_int64_create(64)
			};

			object = rpc_dictionary_create_ex(names, values, 2, false);
			different_object = rpc_dictionary_create();

			rpc_dictionary_set_string(different_object, "string", "another test string");

			THEN("Object contains both items inserted during initialization") {
				REQUIRE(rpc_dictionary_get_count(object) == 2);

				REQUIRE(g_strcmp0(rpc_dictionary_get_string(object, "string"), "test string") == 0);
				REQUIRE(rpc_dictionary_get_int64(object, "int") == 64);

				AND_THEN("Values have refcount set to 2") {
					REQUIRE(rpc_dictionary_get_value(object, "string")->ro_refcnt == 2);
					REQUIRE(rpc_dictionary_get_value(object, "int")->ro_refcnt == 2);
				}
			}

			rpc_release_impl(rpc_dictionary_get_value(object, "string"));
			rpc_release_impl(rpc_dictionary_get_value(object, "int"));
		}

		close(fds[0]);
		close(fds[1]);

		if (dup_fd != 0)
			close(dup_fd);

		if (object != NULL)
			rpc_release(object);

		if (different_object != NULL)
			rpc_release(different_object);
	}
}

SCENARIO("RPC_ARRAY_OBJECT", "Create a ARRAY RPC object and perform basic operations on it") {
	GIVEN("ARRAY object") {
		rpc_object_t object = NULL;
		rpc_object_t different_object = NULL;
		rpc_object_t copy = NULL;
		rpc_object_t value = NULL;
		int data = 0xFF00FF00;
		size_t data_len;
		size_t *data_len_ptr;
		int fds[2];
		int dup_fd = 0;
		struct stat stat1, stat2;

		data_len_ptr = &data_len;
		pipe(fds);

		object = rpc_array_create();
		different_object = rpc_array_create();

		THEN("Empty object has no entries") {
			REQUIRE(rpc_array_get_count(object) == 0);

			REQUIRE(!rpc_array_get_bool(object, 0));
			REQUIRE(rpc_array_get_int64(object, 0) == 0);
			REQUIRE(rpc_array_get_uint64(object, 0) == 0);
			REQUIRE(rpc_array_get_double(object, 0) == 0);
			REQUIRE(rpc_array_get_date(object, 0) == 0);
			REQUIRE(rpc_array_get_data(object, 0, data_len_ptr) == NULL);
			REQUIRE(rpc_array_get_string(object, 0) == NULL);
			REQUIRE(rpc_array_get_fd(object, 0) == 0);
			REQUIRE(rpc_array_dup_fd(object, 0) == 0);

			REQUIRE(rpc_array_get_value(object, 0) == NULL);
		}

		THEN("Type is ARRAY") {
			REQUIRE(rpc_get_type(object) == RPC_TYPE_ARRAY);
		}

		THEN("Refcount equals 1") {
			REQUIRE(object->ro_refcnt == 1);
		}

		WHEN("reference count is incremented") {
			rpc_retain(object);

			THEN("reference count equals 2"){
				REQUIRE(object->ro_refcnt == 2);
			}

			AND_WHEN("reference count is decremented") {
				rpc_release(object);

				THEN("reference count equals 1") {
					REQUIRE(object->ro_refcnt == 1);
				}

				AND_WHEN("reference count reaches 0") {
					rpc_release(object);

					THEN("RPC object pointer is NULL") {
						REQUIRE(object == NULL);
					}
				}
			}
		}

		rpc_release(object);
		rpc_release(different_object);

		WHEN("Array is created") {
			object = rpc_array_create();
			different_object = rpc_array_create();

			rpc_array_set_bool(object, 0, true);
			rpc_array_set_int64(object, 1, -1234);
			rpc_array_set_uint64(object, 2, 1234);
			rpc_array_set_double(object, 3, 12.34);
			rpc_array_set_date(object, 4, 1000);
			rpc_array_set_data(object, 5, &data, sizeof(data));
			rpc_array_set_string(object, 6, "test string");
			rpc_array_set_fd(object, 7, fds[0]);

			rpc_array_set_string(different_object, 0, "value");

			THEN("Array item count matches the number of inserted items") {
				REQUIRE(rpc_array_get_count(object) == 8);
			}

			AND_WHEN("When one of the values is removed") {
				rpc_array_remove_index(object, 1);

				THEN("Array item count is decremented") {
					REQUIRE(rpc_array_get_count(object) == 7);
				}

				THEN("Removed key does not exist in the array anymore") {
					REQUIRE(rpc_array_get_int64(object, 1) != -1234);
				}
			}

			AND_WHEN("One of existing items is being overwritten by NULL") {
				rpc_array_set_value(object, 3, NULL);

				THEN("Key was removed from the array") {
					REQUIRE(rpc_array_get_count(object) == 7);
					REQUIRE(rpc_array_get_int64(object, 3) != -12.34);
				}
			}

			THEN("Extracted bool value matches initial value") {
				REQUIRE(rpc_array_get_bool(object, 0));
			}

			THEN("Extracted integer value matches initial value") {
				REQUIRE(rpc_array_get_int64(object, 1) == -1234);
			}

			THEN("Extracted unsigned integer value matches initial value") {
				REQUIRE(rpc_array_get_uint64(object, 2) == 1234);
			}

			THEN("Extracted double value matches initial value") {
				REQUIRE(rpc_array_get_double(object, 3) == 12.34);
			}

			THEN("Extracted date value matches initial value") {
				REQUIRE(rpc_array_get_date(object, 4) == 1000);
			}

			THEN("Extracted data pointer matches initial data pointer") {
				REQUIRE(rpc_array_get_data(object, 5, data_len_ptr) == &data);
			}

			THEN("Extracted string value matches initial value") {
				REQUIRE(g_strcmp0(rpc_array_get_string(object, 6), "test string") == 0);
			}

			THEN("Extracted fd value matches initial value") {
				REQUIRE(rpc_array_get_fd(object, 7) == fds[0]);
			}

			THEN("Duplicated fd has different value, but references the same file") {
				dup_fd = rpc_array_dup_fd(object, 7);

				REQUIRE(rpc_array_get_fd(object, 7) != dup_fd);
				REQUIRE(fstat(rpc_array_get_fd(object, 7), &stat1) >= 0);
				REQUIRE(fstat(dup_fd, &stat2) >= 0);
				REQUIRE(stat1.st_dev == stat2.st_dev);
				REQUIRE(stat1.st_ino == stat2.st_ino);
			}

			WHEN("Object's copy is created") {
				copy = rpc_copy(object);

				THEN("Source and copy are equal") {
					REQUIRE(rpc_equal(object, copy));
				}

				AND_THEN("Object is different from object initialized with different value") {
					REQUIRE(!rpc_equal(object, different_object));
				}
			}
		}


		WHEN("Object is created with initial values and steals them") {
			const rpc_object_t values[] = {
			    rpc_string_create("test string"),
			    rpc_int64_create(64)
			};

			rpc_object_t object_to_steal;
			rpc_object_t object_to_set;

			object = rpc_array_create_ex(values, 2, true);
			different_object = rpc_array_create();

			rpc_array_set_string(different_object, 0, "another test string");

			THEN("Object contains both items inserted during initialization") {
				REQUIRE(rpc_array_get_count(object) == 2);

				REQUIRE(g_strcmp0(rpc_array_get_string(object, 0), "test string") == 0);
				REQUIRE(rpc_array_get_int64(object, 1) == 64);
			}

			AND_WHEN("Value is stolen") {
				object_to_steal = rpc_dictionary_create();
				rpc_array_steal_value(object, 2, object_to_steal);

				THEN("Entry in array references the original value") {
					REQUIRE(rpc_array_get_value(object, 2) == object_to_steal);

					AND_THEN("Reference count of stolen object remains 1") {
						REQUIRE(rpc_array_get_value(object, 2)->ro_refcnt == 1);
					}
				}
			}

			AND_WHEN("Value is set") {
				object_to_set = rpc_dictionary_create();
				rpc_array_set_value(object, 2, object_to_set);

				THEN("Entry in array references the original value") {
					REQUIRE(rpc_array_get_value(object, 2) == object_to_set);

					AND_THEN("Reference count of the set value was incremented") {
						REQUIRE(rpc_array_get_value(object, 2)->ro_refcnt == 2);
					}
				}

				rpc_release(object_to_set);
			}
		}

		WHEN("Object is created with initial values") {
			const rpc_object_t values[] = {
			    rpc_string_create("test string"),
			    rpc_int64_create(64)
			};

			object = rpc_array_create_ex(values, 2, false);
			different_object = rpc_array_create();

			rpc_array_set_string(different_object, 0, "another test string");

			THEN("Object contains both items inserted during initialization") {
				REQUIRE(rpc_array_get_count(object) == 2);

				REQUIRE(g_strcmp0(rpc_array_get_string(object, 0), "test string") == 0);
				REQUIRE(rpc_array_get_int64(object, 1) == 64);

				AND_THEN("Values have refcount set to 2") {
					REQUIRE(rpc_array_get_value(object, 0)->ro_refcnt == 2);
					REQUIRE(rpc_array_get_value(object, 1)->ro_refcnt == 2);
				}
			}

			rpc_release_impl(rpc_array_get_value(object, 0));
			rpc_release_impl(rpc_array_get_value(object, 1));
		}

		WHEN("Values is appended to an empty object") {
			object = rpc_array_create();
			value = rpc_int64_create(1234);

			AND_WHEN("Value is stolen") {
				rpc_array_append_stolen_value(object, value);

				THEN("Array contains only stolen value") {
					REQUIRE(rpc_array_get_count(object) == 1);
					REQUIRE(rpc_array_get_int64(object, 0) == 1234);
				}

				THEN("Both array and value have refcnt = 1 ") {
					REQUIRE(object->ro_refcnt == 1);
					REQUIRE(value->ro_refcnt == 1);
				}
			}

			AND_WHEN("Value is set") {
				rpc_array_append_value(object, value);

				THEN("Array contains only stolen value") {
					REQUIRE(rpc_array_get_count(object) == 1);
					REQUIRE(rpc_array_get_int64(object, 0) == 1234);
				}

				THEN("Array has refcnt = 1, value has refcnt = 2") {
					REQUIRE(object->ro_refcnt == 1);
					REQUIRE(value->ro_refcnt == 2);
				}

				rpc_release(value);
			}
		}

		close(fds[0]);
		close(fds[1]);

		if (dup_fd != 0)
			close(dup_fd);

		rpc_release(object);
		rpc_release(different_object);
		rpc_release(copy);
	}
}

SCENARIO("RPC_DESCRIPTION_TEST", "Create a tree of RPC objects and print their description") {
	GIVEN("RPC objects tree") {
		int data = 0xff00ff00;
		static const char *referene = "<dictionary> {\n"
		    "    null_val: <null>,\n"
		    "    array: <array> [\n"
		    "        0: <null>,\n"
		    "        1: <bool> true,\n"
		    "        2: <uint64> 1234,\n"
		    "        3: <int64> -1234,\n"
		    "        4: <double> 12.340000,\n"
		    "        5: <date> 1970-01-01 00:00:00,\n"
		    "        6: <string> \"test string\",\n"
		    "        7: <binary> 00ff00ff,\n"
		    "        8: <fd> 10,\n"
		    "    ],\n"
		    "    test_string2: <string> \"test_test_test\",\n"
		    "}";
		char *description = NULL;

		rpc_object_t null = rpc_null_create();
		rpc_object_t boolean = rpc_bool_create(true);
		rpc_object_t u_integer = rpc_uint64_create(1234);
		rpc_object_t integer = rpc_int64_create(-1234);
		rpc_object_t dbl = rpc_double_create(12.34);
		rpc_object_t date = rpc_date_create(0);
		rpc_object_t string = rpc_string_create("test string");
		rpc_object_t binary = rpc_data_create(&data, sizeof(data), false);
		rpc_object_t fd = rpc_fd_create(10);
		rpc_object_t dict = rpc_dictionary_create();
		rpc_object_t array = rpc_array_create();

		rpc_array_append_stolen_value(array, null);
		rpc_array_append_stolen_value(array, boolean);
		rpc_array_append_stolen_value(array, u_integer);
		rpc_array_append_stolen_value(array, integer);
		rpc_array_append_stolen_value(array, dbl);
		rpc_array_append_stolen_value(array, date);
		rpc_array_append_stolen_value(array, string);
		rpc_array_append_stolen_value(array, binary);
		rpc_array_append_stolen_value(array, fd);

		rpc_dictionary_set_value(dict, "null_val", null);
		rpc_dictionary_steal_value(dict, "array", array);

		rpc_dictionary_set_string(dict, "test_string2", "test_test_test");

		THEN("Parent RPC object's description is equal to reference description") {
			description = rpc_copy_description(dict);
			REQUIRE(g_strcmp0(referene, description) == 0);
		}

		rpc_release(dict);
		g_free(description);
	}
}
*/

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
#include <rpc/object.h>
#include "../src/internal.h"
#include <glib.h>


struct {
	rpc_type_t		type;
	rpc_object_t 		object;
	rpc_object_t 		different_object;
	rpc_object_t 		different_type_object;
} generic_object_fixture;

static void
object_generic_test(object_fixture *fixture, gconstpointer user_data)
{
	GIVEN("BOOL object") {
		rpc_object_t object;
		rpc_object_t different_object;
		rpc_object_t copy;
		bool value = true;
		bool different_value = false;
		object = rpc_bool_create(value);
		different_object = rpc_bool_create(different_value);

		THEN("Type is BOOL") {
			REQUIRE(rpc_get_type(object) == RPC_TYPE_BOOL);
		}

		THEN("Refcount equals 1") {
			REQUIRE(object->ro_refcnt == 1);
		}

		THEN("Extracted value matches") {
			REQUIRE(rpc_bool_get_value(object) == value);
		}

		AND_THEN("Direct value matches") {
			REQUIRE(object->ro_value.rv_b == value);
		}

		WHEN("Object's copy is created") {
			copy = rpc_copy(object);

			THEN("Source and copy are equal"){
				REQUIRE(rpc_equal(object, copy));
			}

			AND_THEN("Object is different from object initialized with different value") {
				REQUIRE(!rpc_equal(object, different_object));
			}

			rpc_release(copy);
		}

		WHEN("reference count is incremented") {
			rpc_retain(object);

			THEN("reference count equals 2"){
				REQUIRE(object->ro_refcnt == 2);
				rpc_release(object);
			}

			AND_WHEN("reference count is decremented") {
				rpc_release(object);

				THEN("reference count equals 1") {
					REQUIRE(object->ro_refcnt == 1);
				}

				AND_WHEN("reference count reaches 0") {
					rpc_release(object);

					THEN("RPC object pointer is NULL") {
						REQUIRE(object == NULL);
					}
				}
			}
		}

		if (object != NULL)
			rpc_release(object);

		rpc_release(different_object);
	}
}

static rpc_object_t
object_test_generate_single(rpc_type_t type)
{

}

static rpc_object_t
object_test_generate_nonequal(rpc_object_t object)
{
	uint64_t u_val;
	int64_t i_val;
	double d_val;
	char *s_val;
	size_t s_len;

	switch (object->ro_type) {
	case RPC_TYPE_NULL:
		return object_test_generate_single(RPC_TYPE_INT64);
	case RPC_TYPE_BOOL:
		return rpc_bool_create(~object->ro_value.rv_b);
	case RPC_TYPE_UINT64:
		do
			u_val = RAND_UINT64;
		while (u_val == object->ro_value.rv_ui);
		return rpc_uint64_create(u_val);
	case RPC_TYPE_INT64:
		do
			i_val = RAND_INT64;
		while (i_val == object->ro_value.rv_i);
		return rpc_int64_create(i_val);
	case RPC_TYPE_DOUBLE:
		do
			d_val = g_test_rand_double();
		while (d_val == object->ro_value.rv_d);
		return rpc_double_create(d_val);
	case RPC_TYPE_DATE:
		do
			i_val = RAND_INT64;
		while (i_val == rpc_date_get_value(object));
		return rpc_date_create(i_val);
	case RPC_TYPE_STRING:
		s_len = (size_t)g_test_rand_int_range(1, 100);
		for (int i = 0; i < s_len)
		do
			u_val = RAND_UINT64;
		while (u_val == object->ro_value.rv_ui);
			return rpc_uint64_create(u_val);
	case 'n':
		do
			u_val = RAND_UINT64;
		while (u_val == object->ro_value.rv_ui);
			return rpc_uint64_create(u_val);
	case 'n':
		do
			u_val = RAND_UINT64;
		while (u_val == object->ro_value.rv_ui);
			return rpc_uint64_create(u_val);
	case 'n':
		do
			u_val = RAND_UINT64;
		while (u_val == object->ro_value.rv_ui);
			return rpc_uint64_create(u_val);
	case 'n':
		do
			u_val = RAND_UINT64;
		while (u_val == object->ro_value.rv_ui);
			return rpc_uint64_create(u_val);
	default:
		break;
	}











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
object_test_single_set_up(struct generic_object_fixture *fixture,
    gconstpointer user_data)
{
	rpc_type_t type = *(rpc_type_t *)user_data;
	rpc_type_t rand_type;

	fixture->type = type;

	do
		#if defined(__linux__)
		rand_type = (rpc_type_t)g_test_rand_int_range(RPC_TYPE_NULL,
		    RPC_TYPE_SHMEM);
		#else
		rand_type = (rpc_type_t)g_test_rand_int_range(RPC_TYPE_NULL,
	    	    RPC_TYPE_ERROR);
		#endif
	while (rand_type == type);

	fixture->different_type_object = object_test_generate_single(rand_type);
	fixture->object = object_test_generate_single(type);
	fixture->different_object = object_test_generate_nonequal(
	    fixture->object);
}

static void
object_test_single_tear_down(struct generic_object_fixture *fixture,
    gconstpointer user_data)
{

	rpc_release(fixture->object);
	rpc_release(fixture->different_object);
	rpc_release(fixture->different_type_object);
}

static void
object_test_register()
{

	g_test_add("/api/object/generic/null", struct generic_object_fixture, "json",
	    object_test_single_set_up, serializer_test,
	    object_test_single_tear_down);

}

static struct librpc_test object = {
    .name = "object",
    .register_f = &object_test_register
};

DECLARE_TEST(object);