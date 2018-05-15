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
	struct rpct_typei *value_typei = g_hash_table_lookup(
	    typei->specializations, "T");

	if (value_typei == NULL)
		goto done;

	switch (rpc_get_type(obj)) {
	case RPC_TYPE_ARRAY:
		rpc_array_apply(obj, ^(size_t idx, rpc_object_t value) {
		    struct rpct_error_context newctx;
		    char *name = g_strdup_printf("%lu", idx);

		    rpct_derive_error_context(&newctx, errctx, name);
		    rpct_validate_instance(value_typei, value, &newctx);
		    rpct_release_error_context(&newctx);
		    return ((bool)true);
		});
		break;

	case RPC_TYPE_DICTIONARY:
		rpc_dictionary_apply(obj, ^(const char *key, rpc_object_t value) {
		    struct rpct_error_context newctx;

		    rpct_derive_error_context(&newctx, errctx, key);
		    rpct_validate_instance(value_typei, value, &newctx);
		    rpct_release_error_context(&newctx);
		    return ((bool)true);
		});
		break;

	default:
		break;
	}

done:
	return (rpct_run_validators(typei, obj, errctx));
}

static rpc_object_t
builtin_serialize(rpc_object_t obj)
{

	return (rpc_copy(obj));
}

static struct rpct_class_handler builtin_class_handler = {
	.id = RPC_TYPING_BUILTIN,
	.name = "type",
	.member_fn = NULL,
	.validate_fn = builtin_validate,
	.serialize_fn = builtin_serialize
};

DECLARE_TYPE_CLASS(builtin_class_handler);
