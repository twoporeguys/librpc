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

#include <assert.h>
#include "../linker_set.h"
#include "../internal.h"

static struct rpct_member *
enum_read_member(const char *decl, rpc_object_t obj, struct rpct_type *type)
{
	struct rpct_member *member;
	const char *description = "";

	rpc_object_unpack(obj, "{s}", "description", &description);

	member = g_malloc0(sizeof(*member));
	member->name = g_strdup(decl);
	member->description = g_strdup(description);
	member->origin = type;
	return (member);
}

static bool
enum_validate(struct rpct_typei *typei, rpc_object_t obj,
    struct rpct_error_context *errctx)
{
	struct rpct_member *mem;
	const char *value;

	value = rpc_string_get_string_ptr(obj);
	mem = rpct_type_get_member(typei->type, value);

	if (mem == NULL) {
		rpct_add_error(errctx, "Enum member %s not found", value, NULL);
		return (false);
	}

	return (rpct_run_validators(typei, obj, errctx));
}

static rpc_object_t
enum_serialize(rpc_object_t obj)
{

	assert(obj != NULL);
	assert(obj->ro_typei != NULL);
	assert(rpc_get_type(obj) == RPC_TYPE_STRING);

	return (rpc_object_pack("{s,s,v}",
	    RPCT_REALM_FIELD, obj->ro_typei->type->realm,
	    RPCT_TYPE_FIELD, obj->ro_typei->canonical_form,
	    RPCT_VALUE_FIELD, obj));
}

static struct rpct_class_handler enum_class_handler = {
	.id = RPC_TYPING_ENUM,
	.name = "enum",
	.member_fn = enum_read_member,
	.validate_fn = enum_validate,
	.serialize_fn = enum_serialize,
};

DECLARE_TYPE_CLASS(enum_class_handler);
