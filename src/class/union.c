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

#include <errno.h>
#include <assert.h>
#include "../linker_set.h"
#include "../internal.h"

static struct rpct_member *union_read_member(const char *decl,
    rpc_object_t obj, struct rpct_type *type)
{
	struct rpct_member *member;
	const char *typedecl = NULL;
	const char *description = "";
	rpc_object_t constraints = NULL;

	rpc_object_unpack(obj, "{s,s,v}",
	    "type", &typedecl,
	    "description", &description,
	    "constraints", &constraints);

	if (typedecl == NULL) {
		rpc_set_last_error(EINVAL, "type key not provided or invalid", NULL);
		return (NULL);
	}

	member = g_malloc0(sizeof(*member));
	member->name = g_strdup(decl);
	member->description = description != NULL ? g_strdup(description) : NULL;
	member->origin = type;
	member->type = rpct_instantiate_type(typedecl, NULL, type);
	member->constraints = g_hash_table_new_full(g_str_hash, g_str_equal,
	    g_free, (GDestroyNotify)rpc_release_impl);

	if (constraints != NULL) {
		rpc_dictionary_apply(constraints, ^(const char *key, rpc_object_t value) {
		    g_hash_table_insert(member->constraints, g_strdup(key),
		        value);
		    return ((bool)true);
		});
	}

	return (member);
}

static rpc_object_t
union_serialize(rpc_object_t obj)
{

	assert(obj != NULL);
	assert(obj->ro_typei != NULL);

	return (rpc_object_pack("{s,v}",
	    RPCT_TYPE_FIELD, obj->ro_typei->canonical_form,
	    RPCT_VALUE_FIELD, obj));
}

static bool
union_validate(struct rpct_typei *typei, rpc_object_t obj,
    struct rpct_error_context *errctx)
{

	return (rpct_run_validators(typei, obj, errctx));
}

static struct rpct_class_handler union_class_handler = {
	.id = RPC_TYPING_UNION,
	.name = "union",
	.member_fn = union_read_member,
	.validate_fn = union_validate,
	.serialize_fn = union_serialize
};

DECLARE_TYPE_CLASS(union_class_handler);
