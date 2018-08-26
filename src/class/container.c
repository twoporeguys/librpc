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
 * DIRECT, INDIRECT\\, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include "../linker_set.h"
#include "../internal.h"

static bool
container_validate(struct rpct_typei *typei, rpc_object_t obj,
    struct rpct_error_context *errctx)
{
	rpct_typei_t value_typei;
	bool fail;

	/*
	 * typei->type->value_type might be a generic variable reference,
	 * eg. "T" so we must instantiate it in context of a parent type.
	 */
	value_typei = rpct_instantiate_type(
	    typei->type->value_type->canonical_form, typei, typei->type,
	    typei->type->file);

	switch (rpc_get_type(obj)) {
	case RPC_TYPE_ARRAY:
		fail = rpc_array_apply(obj, ^(size_t idx, rpc_object_t value) {
			struct rpct_error_context newctx;
			char *name = g_strdup_printf("%zu", idx);
			bool ret;

			rpct_derive_error_context(&newctx, errctx, name);
			ret = rpct_validate_instance(value_typei, value, &newctx);
			rpct_release_error_context(&newctx);
			g_free(name);
			return (ret);
		});
		break;

	case RPC_TYPE_DICTIONARY:
		fail = rpc_dictionary_apply(obj, ^(const char *key, rpc_object_t value) {
			struct rpct_error_context newctx;
			bool ret;

			rpct_derive_error_context(&newctx, errctx, key);
			ret = rpct_validate_instance(value_typei, value, &newctx);
			rpct_release_error_context(&newctx);
			return (ret);
		});
		break;

	default:
		rpct_add_error(errctx, NULL, "Non-container value");
		fail = true;
		break;
	}

	if (fail)
		return (false);

	//rpct_typei_release(value_typei);
	return (rpct_run_validators(typei, obj, errctx));
}

static rpc_object_t
container_serialize(rpc_object_t obj)
{
	rpc_object_t value;

	assert(obj != NULL);
	assert(obj->ro_typei != NULL);

	switch (rpc_get_type(obj)) {
	case RPC_TYPE_ARRAY:
		value = rpc_array_create();
		rpc_array_apply(obj, ^(size_t idx, rpc_object_t v) {
			rpc_array_append_stolen_value(value, rpct_serialize(v));
			return ((bool)true);
		});
		break;

	case RPC_TYPE_DICTIONARY:
		value = rpc_dictionary_create();
		rpc_dictionary_apply(obj, ^(const char *k, rpc_object_t v) {
			rpc_dictionary_steal_value(value, k, rpct_serialize(v));
			return ((bool)true);
		});
		break;

	default:
		g_assert_not_reached();
	}

	return (rpc_object_pack("{s,v}",
	    RPCT_TYPE_FIELD, obj->ro_typei->canonical_form,
	    RPCT_VALUE_FIELD, value));
}

static rpc_object_t
container_deserialize(rpc_object_t obj)
{
	rpc_object_t result;
	obj = rpc_dictionary_get_value(obj, RPCT_VALUE_FIELD);

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

	g_assert_not_reached();
}

static struct rpct_class_handler container_class_handler = {
	.id = RPC_TYPING_CONTAINER,
	.name = "container",
	.member_fn = NULL,
	.validate_fn = container_validate,
	.serialize_fn = container_serialize,
	.deserialize_fn = container_deserialize
};

DECLARE_TYPE_CLASS(container_class_handler);
