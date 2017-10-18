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

#include <inttypes.h>
#include "../linker_set.h"
#include "../internal.h"

static bool
validate_int64_range(rpc_object_t obj, rpc_object_t params,
    struct rpct_typei *typei __unused, struct rpct_error_context *errctx)
{
	int64_t min = -1;
	int64_t max = -1;
	int64_t value;
	bool valid = true;

	rpc_object_unpack(params, "{i,i}", "min", &min, "max", &max);
	value = rpc_int64_get_value(obj);

	if (max != -1 && value > max) {
		valid = false;
		rpct_add_error(errctx,
		    "Value %" PRId64 " is larger than the maximum allowed: %" PRId64,
		    value, max, NULL);
	}

	if (min != -1 && value < min) {
		valid = false;
		rpct_add_error(errctx,
		    "Value %" PRId64 " is smaller than the minimum allowed: %" PRId64,
		    value, max, NULL);
	}

	return (valid);
}

struct rpct_validator validator_int64_range = {
	.type = "int64",
	.name = "range",
	.validate = validate_int64_range
};

DECLARE_VALIDATOR(validator_int64_range);
