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

#include <glib/gprintf.h>

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

		rpc_release(different_object);
	}
}


