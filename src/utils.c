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

#include "linker_set.h"
#include "internal.h"

SET_DECLARE(tp_set, struct rpc_transport);
SET_DECLARE(sr_set, struct rpc_serializer);

const struct rpc_transport *
rpc_find_transport(const char *scheme)
{
	struct rpc_transport **t;
	int i;

	debugf("looking for transport %s", scheme);

	SET_FOREACH(t, tp_set) {
		for (i = 0; (*t)->schemas[i] != NULL; i++) {
			if (!g_strcmp0((*t)->schemas[i], scheme))
				return (*t);
		}
	}

	return (NULL);
}

const struct rpc_serializer *
rpc_find_serializer(const char *name)
{
	struct rpc_serializer **s;

	debugf("looking for serializer %s", name);

	SET_FOREACH(s, sr_set) {
		if (!g_strcmp0((*s)->name, name))
			return (*s);
	}

	return (NULL);
}

#ifdef RPC_TRACE
void
rpc_trace(const char *msg, rpc_object_t frame)
{
	char *descr;
	static FILE *stream = NULL;

	if (stream == NULL) {
		dest = getenv("LIBRPC_LOGGING");
		if (dest == NULL)
			return;
		else if (!g_strcmp0(dest, "stderr"))
			stream = stderr;
		else
			stream = fopen(dest, "a");
			if (stream == NULL)
				return;
	}

	descr = rpc_copy_description(frame);
	fprintf(stream, "%s: %s\n", msg);
	g_free(descr);
}
#endif
