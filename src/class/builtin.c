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
 */

#include "../linker_set.h"
#include "../internal.h"

static bool
builtin_validate(struct rpct_typei *typei, rpc_object_t obj,
    struct rpct_error_context *errctx)
{

	return (rpct_run_validators(typei, obj, errctx));
}

static rpc_object_t
builtin_serialize(rpc_object_t obj)
{

	return (rpc_copy(obj));
}

static rpc_object_t
builtin_deserialize(rpc_object_t obj)
{
	rpc_object_t result;

	if (rpc_get_type(obj) == RPC_TYPE_DICTIONARY) {
		result = rpc_dictionary_create();
		rpc_dictionary_apply(obj, ^(const char *key, rpc_object_t value) {
			rpc_dictionary_steal_value(result, key, rpct_deserialize(value));
			return ((bool)true);
		});

		return (result);
	}

	if (rpc_get_type(obj) == RPC_TYPE_ARRAY) {
		result = rpc_array_create();
		rpc_array_apply(obj, ^(size_t i __unused, rpc_object_t value) {
			rpc_array_append_stolen_value(result, rpct_deserialize(value));
			return ((bool)true);
		});

		return (result);
	}

	return (rpc_copy(obj));
}


static struct rpct_class_handler builtin_class_handler = {
	.id = RPC_TYPING_BUILTIN,
	.name = "type",
	.member_fn = NULL,
	.validate_fn = builtin_validate,
	.serialize_fn = builtin_serialize,
	.deserialize_fn = builtin_deserialize
};

DECLARE_TYPE_CLASS(builtin_class_handler);
