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

#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <rpc/object.h>
#include "../../src/internal.h"

static rpc_object_t create_string_with_arguments(const char *fmt, ...) {
	rpc_object_t object;
	va_list ap;

	va_start(ap, fmt);
	object = rpc_string_create_with_format_and_arguments("%s", ap);
	va_end(ap);

	return object;
}

SCENARIO("RPC_NULL_OBJECT", "Create a NULL RPC object and perform basic operations on it") {
	GIVEN("BOOL object") {
		rpc_object_t object = NULL;
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

SCENARIO("RPC_DATE_OBJECT", "Create a DATE RPC object and perform basic operations on it") {
	GIVEN("DATE object") {
		rpc_object_t object = NULL;
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

		rpc_release(object);
		rpc_release(different_object);
	}
}

SCENARIO("RPC_STRING_OBJECT", "Create a STRING RPC object and perform basic operations on it") {
	GIVEN("STRING object") {
		rpc_object_t object = NULL;
		rpc_object_t different_object = NULL;
		rpc_object_t copy = NULL;
		static const char *value = "first test string";
		static const char *different_value = "second test string";


		WHEN("Objects are created with formatting") {
			object = rpc_string_create_with_format("%s", value);
			different_object = rpc_string_create_with_format("%s", different_value);

			THEN("Length of object and original string are the same") {
				REQUIRE(strlen(value) == rpc_string_get_length(object));
			}

			THEN("Extracted value matches") {
				REQUIRE(g_strcmp0(rpc_string_get_string_ptr(object), value) == 0);
			}

			AND_WHEN("Object's copy is created") {
				copy = rpc_copy(object);

				THEN("Source and copy are equal") {
					REQUIRE(rpc_equal(object, copy));
				}

				AND_THEN("Object is different from object initialized with different value") {
					REQUIRE(!rpc_equal(object, different_object));
				}
			}
		}

		WHEN("Objects are created with formatting and arguments") {
			object = create_string_with_arguments("%s", value);
			different_object = create_string_with_arguments("%s", different_value);

			THEN("Length of object and original string are the same") {
				REQUIRE(strlen(value) == rpc_string_get_length(object));
			}

			THEN("Extracted value matches") {
				REQUIRE(g_strcmp0(rpc_string_get_string_ptr(object), value) == 0);
			}

			AND_WHEN("Object's copy is created") {
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

		WHEN("Objects are created from value") {
			object = rpc_string_create(value);
			different_object = rpc_string_create(different_value);


			THEN("Type is STRING") {
				REQUIRE(rpc_get_type(object) == RPC_TYPE_STRING);
			}

			THEN("Refcount equals 1") {
				REQUIRE(object->ro_refcnt == 1);
			}

			THEN("Length of object and original string are the same") {
				REQUIRE(strlen(value) == rpc_string_get_length(object));
			}

			THEN("Extracted value matches") {
				REQUIRE(g_strcmp0(rpc_string_get_string_ptr(object), value) == 0);
			}

			AND_WHEN("Object's copy is created") {
				copy = rpc_copy(object);

				THEN("Source and copy are equal") {
					REQUIRE(rpc_equal(object, copy));
				}

				AND_THEN("Object is different from object initialized with different value") {
					REQUIRE(!rpc_equal(object, different_object));
				}

			}

			AND_WHEN("reference count is incremented") {
				rpc_retain(object);

				THEN("reference count equals 2") {
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
		}

		rpc_release(object);
		rpc_release(different_object);
		rpc_release(copy);
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

SCENARIO("RPC_FD_OBJECT", "Create a FD RPC object and perform basic operations on it") {
	GIVEN("FD object") {
		rpc_object_t object = NULL;
		rpc_object_t different_object = NULL;
		rpc_object_t copy = NULL;
		int fds[2];
		int dup_fd = 0;
		struct stat stat1, stat2;

		pipe(fds);

		object = rpc_fd_create(fds[0]);
		different_object = rpc_fd_create(fds[1]);

		THEN("Type is FD") {
			REQUIRE(rpc_get_type(object) == RPC_TYPE_FD);
		}

		THEN("Refcount equals 1") {
			REQUIRE(object->ro_refcnt == 1);
		}

		THEN("Extracted value matches") {
			REQUIRE(rpc_fd_get_value(object) == fds[0]);
		}

		AND_THEN("Direct value matches") {
			REQUIRE(object->ro_value.rv_fd == fds[0]);
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

		WHEN("File descriptor is duplicated") {
			dup_fd = rpc_fd_dup(object);

			THEN("Both RPC object and duplicate file descriptor are pointing to the same file") {
				REQUIRE(fstat(rpc_fd_get_value(object), &stat1) >= 0);
				REQUIRE(fstat(dup_fd, &stat2) >= 0);
				REQUIRE(stat1.st_dev == stat2.st_dev);
				REQUIRE(stat1.st_ino == stat2.st_ino);
			}
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

		close(fds[0]);
		close(fds[1]);

		if (dup_fd != 0)
			close(dup_fd);

		rpc_release(object);
		rpc_release(copy);
		rpc_release(different_object);
	}
}

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

		THEN("Parent RPC object's description is equal to refrence description") {
			description = rpc_copy_description(dict);
			REQUIRE(g_strcmp0(referene, description) == 0);
		}

		rpc_release(dict);
		g_free(description);
	}
}
