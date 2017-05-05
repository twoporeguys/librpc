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

SCENARIO("RPC_OBJECT_CREATE", "Create RPC object and check its internal value") {
        GIVEN("Integer initialized RPC object") {
                rpc_object_t object;
                int value = 10;

                object = rpc_int64_create(value);
                REQUIRE(rpc_get_type(object) == RPC_TYPE_INT64);
                REQUIRE(rpc_int64_get_value(object) == value);
                REQUIRE(object->ro_refcnt == 1);
                REQUIRE(object->ro_value.rv_i == value);

                WHEN("reference count is incremented") {
                        rpc_retain(object);

                        THEN("reference count equals 2"){
                                REQUIRE(object->ro_refcnt == 2);
                        }

                        AND_WHEN("reference count is decremented") {
                                rpc_release(object);

                                AND_THEN("reference count equals 1") {
                                        REQUIRE(object->ro_refcnt == 1);
                                }
                        }
                }
        }
}
