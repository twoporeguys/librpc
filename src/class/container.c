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

#include "../linker_set.h"
#include "../internal.h"

static bool
container_validate(struct rpct_typei *typei, rpc_object_t obj,
    struct rpct_error_context *errctx)
{

	if (!rpct_validate_instance(typei->type->definition, obj, errctx))
		return (false);

	return (rpct_run_validators(typei, obj, errctx));
}

static rpc_object_t
container_serialize(rpc_object_t obj)
{

	return (rpc_copy(obj));
}

static rpc_object_t
container_deserialize(rpc_object_t obj)
{

	return (rpc_copy(obj));
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
