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

#include "catch.hpp"

#include <rpc/object.h>
#include "../src/internal.h"

SCENARIO("RPC_NULL_OBJECT", "Create a NULL RPC object and perform basic operations on it") {
	GIVEN("BOOL object") {
		rpc_object_t object;
		rpc_object_t copy;
		object = rpc_null_create();

		THEN("Type is NULL") {
			REQUIRE(rpc_get_type(object) == RPC_TYPE_NULL);
		}

		AND_THEN("Refcount equals 1") {
			REQUIRE(object->ro_refcnt == 1);
		}

		WHEN("Object's copy is created") {
			copy = rpc_copy(object);

			THEN("Source and copy are equal"){
				REQUIRE(rpc_equal(object, copy));
			}

			rpc_release(copy);
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
	}
}

SCENARIO("RPC_BOOL_OBJECT", "Create a BOOL RPC object and perform basic operations on it") {
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

SCENARIO("RPC_UINT64_OBJECT", "Create a UINT64 RPC object and perform basic operations on it") {
	GIVEN("UINT64 object") {
		rpc_object_t object;
		rpc_object_t different_object;
		rpc_object_t copy;
		uint64_t value = 1234;
		uint64_t different_value = 5678;
		object = rpc_uint64_create(value);
		different_object = rpc_uint64_create(different_value);

		THEN("Type is UINT64") {
			REQUIRE(rpc_get_type(object) == RPC_TYPE_UINT64);
		}

		THEN("Refcount equals 1") {
			REQUIRE(object->ro_refcnt == 1);
		}

		THEN("Extracted value matches") {
			REQUIRE(rpc_uint64_get_value(object) == value);
		}

		AND_THEN("Direct value matches") {
			REQUIRE(object->ro_value.rv_ui == value);
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

SCENARIO("RPC_INT64_OBJECT", "Create a INT64 RPC object and perform basic operations on it") {
	GIVEN("INT64 object") {
		rpc_object_t object;
		rpc_object_t different_object;
		rpc_object_t copy;
		int64_t value = 1234;
		int64_t different_value = -1234;
		object = rpc_int64_create(value);
		different_object = rpc_int64_create(different_value);

		THEN("Type is INT64") {
			REQUIRE(rpc_get_type(object) == RPC_TYPE_INT64);
		}

		THEN("Refcount equals 1") {
			REQUIRE(object->ro_refcnt == 1);
		}

		THEN("Extracted value matches") {
			REQUIRE(rpc_int64_get_value(object) == value);
		}

		AND_THEN("Direct value matches") {
			REQUIRE(object->ro_value.rv_i == value);
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

SCENARIO("RPC_DOUBLE_OBJECT", "Create a DOUBLE RPC object and perform basic operations on it") {
	GIVEN("DOUBLE object") {
		rpc_object_t object;
		rpc_object_t different_object;
		rpc_object_t copy;
		double value = 12.34;
		double different_value = -12.34;
		object = rpc_double_create(value);
		different_object = rpc_double_create(different_value);

		THEN("Type is DOUBLE") {
			REQUIRE(rpc_get_type(object) == RPC_TYPE_DOUBLE);
		}

		THEN("Refcount equals 1") {
			REQUIRE(object->ro_refcnt == 1);
		}

		THEN("Extracted value matches") {
			REQUIRE(rpc_double_get_value(object) == value);
		}

		AND_THEN("Direct value matches") {
			REQUIRE(object->ro_value.rv_d == value);
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

SCENARIO("RPC_DATE_OBJECT", "Create a DATE RPC object and perform basic operations on it") {
	GIVEN("DATE object") {
		rpc_object_t object;
		rpc_object_t different_object;
		rpc_object_t copy;
		int value = 0;
		object = rpc_date_create(value);
		different_object = rpc_date_create_from_current();

		THEN("Type is DATE") {
			REQUIRE(rpc_get_type(object) == RPC_TYPE_DATE);
		}

		THEN("Refcount equals 1") {
			REQUIRE(object->ro_refcnt == 1);
		}

		THEN("Extracted value matches") {
			REQUIRE(rpc_date_get_value(object) == value);
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

SCENARIO("RPC_STRING_OBJECT", "Create a STRING RPC object and perform basic operations on it") {
	GIVEN("STRING object") {
		rpc_object_t object;
		rpc_object_t different_object;
		rpc_object_t copy;
		static const char *value = "first test string";
		static const char *different_value = "second test string";
		object = rpc_string_create(value);
		different_object = rpc_string_create(different_value);

		THEN("Type is STRING") {
			REQUIRE(rpc_get_type(object) == RPC_TYPE_STRING);
		}

		THEN("Refcount equals 1") {
			REQUIRE(object->ro_refcnt == 1);
		}

		THEN("Extracted value matches") {
			REQUIRE(g_strcmp0(rpc_string_get_string_ptr(object), value) == 0);
		}

		WHEN("Object's copy is created") {
			copy = rpc_copy(object);

			THEN("Source and copy are equal"){
				REQUIRE(rpc_equal(object, copy));
			}

			AND_THEN("Object is different from object initialized with different value") {
				REQUIRE(!rpc_equal(object, different_object));
			}
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
	}
}

SCENARIO("RPC_BINARY_OBJECT", "Create a BINARY RPC object and perform basic operations on it") {
	GIVEN("BINARY object") {
		rpc_object_t object;
		rpc_object_t different_object;
		rpc_object_t copy;
		int value = 0xff00ff00;
		int different_value = 0xabcdef00;
		int *buffer = (int *)malloc(sizeof(value));
		object = rpc_data_create(&value, sizeof(value), true);
		different_object = rpc_data_create(&different_value, sizeof(different_value), true);

		THEN("Type is BINARY") {
			REQUIRE(rpc_get_type(object) == RPC_TYPE_BINARY);
		}

		THEN("Refcount equals 1") {
			REQUIRE(object->ro_refcnt == 1);
		}

		THEN("Object is referencing a copy of inital data") {
			REQUIRE(object->ro_value.rv_bin.copy);
			REQUIRE(rpc_data_get_bytes_ptr(object) != &value);
		}

		THEN("Length of data inside of the object is the same as length of initial data") {
			REQUIRE(object->ro_value.rv_bin.length == sizeof(value));
		}

		THEN("Extracted value matches") {
			rpc_data_get_bytes(object, (void *)buffer, 0, sizeof(value));
			REQUIRE(buffer[0] == value);
		}

		WHEN("Object's copy is created") {
			copy = rpc_copy(object);

			THEN("Source and copy are equal"){
				REQUIRE(rpc_equal(object, copy));
			}

			AND_THEN("Object and its copy are referencing different buffers") {
				REQUIRE(copy->ro_value.rv_bin.copy);
				REQUIRE(rpc_data_get_bytes_ptr(object) != rpc_data_get_bytes_ptr(copy));
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

		WHEN("Object is reinitialized as a reference to initial data") {
			rpc_release(object);
			object = rpc_data_create(&value, sizeof(value), false);

			THEN("Refcount equals 1") {
				REQUIRE(object->ro_refcnt == 1);
			}

			THEN("Object is referencing inital data") {
				REQUIRE(!object->ro_value.rv_bin.copy);
				REQUIRE(rpc_data_get_bytes_ptr(object) == &value);
			}

			THEN("Length of data inside of the object is the same as length of initial data") {
				REQUIRE(object->ro_value.rv_bin.length == sizeof(value));
			}

			THEN("Extracted value matches") {
				rpc_data_get_bytes(object, (void *)buffer, 0, sizeof(value));
				REQUIRE(buffer[0] == value);
			}

			AND_WHEN("reference count reaches 0") {
				rpc_release(object);

				THEN("RPC object pointer is NULL") {
					REQUIRE(object == NULL);
				}
			}
		}

		if (object != NULL)
			rpc_release(object);

		rpc_release(different_object);
		g_free(buffer);
	}
}

SCENARIO("RPC_DESCRIPTION_TEST", "Create a tree of RPC objects and print their description") {
	GIVEN("RPC objects tree") {
		int data = 0xff00ff00;
		static const char *referene = "<dictionary> {\n"
		    "    null_val: <null> ,\n"
		    "    array: <array> [\n"
		    "        0: <null> ,\n"
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
		    "}\n";

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
		rpc_dictionary_set_value(dict, "array", array);

		rpc_dictionary_set_string(dict, "test_string2", "test_test_test");

		THEN("Parent RPC object's description is equal to refrence description") {
			REQUIRE(g_strcmp0(referene, rpc_copy_description(dict)) == 0);
		}

		rpc_release(dict);
	}
}
