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
 */

#include <glib.h>
#include "../linker_set.h"
#include "../internal.h"

static bool
validate_string_regex(rpc_object_t obj, rpc_object_t params,
    struct rpct_typei *typei __unused, struct rpct_error_context *errctx)
{
	bool valid = true;
	const char *pattern;
	const char *str;

	rpc_object_unpack(params, "{s}", "pattern", &pattern);
	str = rpc_string_get_string_ptr(obj);

	if (!g_regex_match_simple(pattern, str, 0, 0)) {
		valid = false;
		rpct_add_error(errctx, "String doesn't match", NULL);
	}

	return (valid);
}

struct rpct_validator validator_string_regex = {
	.type = "string",
	.name = "regex",
	.validate = validate_string_regex
};

DECLARE_VALIDATOR(validator_string_regex);
