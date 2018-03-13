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

#include <stdlib.h>
#ifndef _WIN32
#include <execinfo.h>
#endif
#include <glib.h>
#include "linker_set.h"
#include "internal.h"

SET_DECLARE(tp_set, struct rpc_transport);
SET_DECLARE(sr_set, struct rpc_serializer);
SET_DECLARE(vr_set, struct rpct_validator);
SET_DECLARE(cs_set, struct rpct_class_handler);
static GPrivate rpc_last_error = G_PRIVATE_INIT((GDestroyNotify)rpc_release_impl);

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

const struct rpct_validator *
rpc_find_validator(const char *type, const char *name)
{
	struct rpct_validator **s;

	debugf("looking for validator %s", name);

	SET_FOREACH(s, vr_set) {
		if (!g_strcmp0((*s)->name, name) && !g_strcmp0((*s)->type, type))
			return (*s);
	}

	return (NULL);
}

const struct rpct_class_handler *
rpc_find_class_handler(const char *name, rpct_class_t cls)
{
	struct rpct_class_handler **s;

	SET_FOREACH(s, cs_set) {
		if (name != NULL && !g_strcmp0(name, (*s)->name))
			return (*s);

		if (name == NULL && cls == (*s)->id)
			return (*s);
	}

	return (NULL);
}

void
rpc_set_last_error(int code, const char *msg, rpc_object_t extra)
{
	rpc_object_t error;

	error = rpc_error_create(code, msg, extra);
	g_private_replace(&rpc_last_error, error);
}

void
rpc_set_last_errorf(int code, const char *fmt, ...)
{
	va_list ap;
	rpc_object_t error;
	char *msg;

	va_start(ap, fmt);
	msg = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	error = rpc_error_create(code, msg, NULL);
	g_free(msg);
	g_private_replace(&rpc_last_error, error);
}


void
rpc_set_last_gerror(GError *g_error)
{
	rpc_object_t error;

	error = rpc_error_create_from_gerror(g_error);
	g_private_replace(&rpc_last_error, error);
}


void
rpc_set_last_rpc_error(rpc_object_t rpc_error)
{

	if (rpc_get_type(rpc_error) != RPC_TYPE_ERROR)
		return;
        g_private_replace(&rpc_last_error, rpc_error);
}


rpc_object_t
rpc_get_last_error(void)
{
	rpc_object_t error;

	error = g_private_get(&rpc_last_error);
	return (error);
}

void
rpc_trace(const char *msg, rpc_object_t frame)
{
	char *descr;
	const char *dest;
	static FILE *stream = NULL;
	GDateTime *now;

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

	now = g_date_time_new_now_local();
	descr = rpc_copy_description(frame);
	fprintf(stream, "[%02d:%02d:%02d.%06d] %s: %s\n",
	    g_date_time_get_hour(now), g_date_time_get_minute(now),
	    g_date_time_get_second(now), g_date_time_get_microsecond(now),
	    msg, descr);

	g_date_time_unref(now);
	g_free(descr);
}

#ifdef _WIN32
char *
rpc_get_backtrace(void)
{

	return (NULL);
}
#else
char *
rpc_get_backtrace(void)
{
	GString *result;
	int count, i;
	void *buffer[128];
	char **names;

	count = backtrace(buffer, 128);
	if (count == 0)
		return (NULL);

	names = backtrace_symbols(buffer, count);
	if (names == NULL)
		return (NULL);

	result = g_string_new("Traceback (most recent call first):\n");

	for (i = 1; i < count; i++)
		g_string_append_printf(result, "%s\n", names[i]);

	free(names);
	return (g_string_free(result, false));
}
#endif

char *
rpc_generate_v4_uuid(void)
{
	uint8_t bytes[16];
	int i;

	for (i = 0; i < 16; i++)
		bytes[i] = (uint8_t)g_random_int_range(0, 256);

	bytes[6] &= 0x0f;
	bytes[6] |= 4 << 4; /* version 4 */
	bytes[8] &= 0x3f;
	bytes[8] |= 0x80;

	return (g_strdup_printf("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x"
	    "-%02x%02x%02x%02x%02x%02x", bytes[0], bytes[1], bytes[2],
	    bytes[3], bytes[4], bytes[5], bytes[6], bytes[7], bytes[8],
	    bytes[9], bytes[10], bytes[11], bytes[12], bytes[13],
	    bytes[14], bytes[15]));
}

gboolean
rpc_kill_main_loop(void *arg)
{
	GMainLoop *loop = arg;

	g_main_loop_quit(loop);
	return (false);
}

int
rpc_ptr_array_string_index(GPtrArray *arr, const char *str)
{

	for (guint i = 0; i < arr->len; i++) {
		if (g_strcmp0(g_ptr_array_index(arr, i), str) == 0)
			return (i);
	}

	return (-1);
}
