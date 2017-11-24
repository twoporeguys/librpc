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

#include <rpc/interface.h>
#include "internal.h"

rpc_instance_t rpc_instance_new(const char *path, void *arg)
{
	rpc_instance_t result;

	result = g_malloc0(sizeof(result));
	result->ri_path = g_strdup(path);
	result->ri_methods = g_hash_table_new(g_str_hash, g_str_equal);
	result->ri_arg = arg;

	return (result);
}

void *rpc_instance_get_arg(rpc_instance_t instance)
{

	g_assert_nonnull(instance);

	return (instance->ri_arg);
}

const char *rpc_instance_get_path(rpc_instance_t instance)
{

	g_assert_nonnull(instance);

	return (instance->ri_path);
}

void rpc_instance_free(rpc_instance_t instance)
{
	g_assert_nonnull(instance);

	g_free(instance->ri_path);
	g_hash_table_destroy(instance->ri_methods);
	g_free(instance);
}
