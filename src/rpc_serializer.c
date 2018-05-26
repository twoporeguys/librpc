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

#include <errno.h>
#include <rpc/serializer.h>
#include "internal.h"

bool
rpc_serializer_exists(const char *serializer)
{

	return (rpc_find_serializer(serializer) != NULL ? true : false);
}

rpc_object_t
rpc_serializer_load(const char *serializer, const void *frame,
    size_t len)
{
	const struct rpc_serializer *impl;
	rpc_auto_object_t untyped = NULL;

	impl = rpc_find_serializer(serializer);
	if (impl == NULL) {
		rpc_set_last_error(ENOENT, "Serializer not found", NULL);
		return (NULL);
	}

	untyped = impl->deserialize(frame, len);
	if (untyped == NULL)
		return (NULL);

	return (rpct_deserialize(untyped));
}

int
rpc_serializer_dump(const char *serializer, rpc_object_t obj, void **framep,
    size_t *lenp)
{
	const struct rpc_serializer *impl;
	rpc_auto_object_t typed = NULL;

	impl = rpc_find_serializer(serializer);
	if (impl == NULL) {
		rpc_set_last_error(ENOENT, "Serializer not found", NULL);
		return (-1);
	}

	typed = rpct_serialize(obj);
	if (typed == NULL)
		return (-1);

	return (impl->serialize(typed, framep, lenp));
}
