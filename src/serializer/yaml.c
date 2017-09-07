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

#include <string.h>
#include <glib.h>
#include <rpc/object.h>
#include <yaml.h>
#include "../linker_set.h"
#include "../internal.h"
#include "yaml.h"

static int
rpc_yaml_serialize(rpc_object_t obj, void **frame, size_t *size)
{

}

static rpc_object_t
rpc_yaml_read_scalar(yaml_event_t *event)
{
	char *value = (char *)event->data.scalar.value;
	size_t len = event->data.scalar.length;
	int64_t intval;
	rpc_object_t ret;
	char *endptr;

	if (strncmp(value, "null", len) == 0) {
		ret = rpc_null_create();
		goto done;
	}

	if (strncmp(value, "true", len) == 0) {
		ret = rpc_bool_create(true);
		goto done;
	}

	if (strncmp(value, "false", len) == 0) {
		ret = rpc_bool_create(false);
		goto done;
	}

	intval = (int64_t)g_ascii_strtoll(value, &endptr, 10);
	if (*endptr == '\0') {
		ret = rpc_int64_create(intval);
		goto done;
	}

	if (value[0] == '"' && value[len-1] == '"') {
		ret = rpc_string_create_len(&value[1], len - 2);
		goto done;
	}

	ret = rpc_string_create_len(value, len);
done:
	ret->ro_column = event->start_mark.column;
	ret->ro_line = event->start_mark.line;
	return (ret);
}

static rpc_object_t
rpc_yaml_deserialize(const void *frame, size_t size)
{
	GQueue *containers = g_queue_new();
	GQueue *keys = g_queue_new();
	rpc_object_t current = NULL;
	rpc_object_t container;
	yaml_parser_t parser;
	yaml_event_t event;
	bool done = false;
	bool read_key = false;
	char *key;

	yaml_parser_initialize(&parser);
	yaml_parser_set_input_string(&parser, frame, size);

	while (!done) {
		if (!yaml_parser_parse(&parser, &event))
			goto out;

		switch (event.type) {
			case YAML_MAPPING_START_EVENT:
				read_key = true;
				g_queue_push_head(containers,
				    rpc_dictionary_create());
				goto done;

			case YAML_MAPPING_END_EVENT:
				current = g_queue_pop_head(containers);
				break;

			case YAML_SEQUENCE_START_EVENT:
				g_queue_push_head(containers, rpc_array_create());
				goto done;

			case YAML_SEQUENCE_END_EVENT:
				current = g_queue_pop_head(containers);
				break;

			case YAML_SCALAR_EVENT:
				if (read_key) {
					key = g_strndup((const char *)
					    event.data.scalar.value,
					    event.data.scalar.length);

					read_key = false;
					g_queue_push_head(keys, key);
					goto done;

				}
				current = rpc_yaml_read_scalar(&event);
				break;

			case YAML_STREAM_END_EVENT:
				done = true;
				goto done;

			default:
				debugf("ignored event %d", event.type)
				goto done;
		}

		container = g_queue_peek_head(containers);
		if (container == NULL)
			goto out;

		switch (rpc_get_type(container)) {
		case RPC_TYPE_ARRAY:
			rpc_array_append_stolen_value(container, current);
			break;

		case RPC_TYPE_DICTIONARY:
			rpc_dictionary_set_value(container,
			    g_queue_pop_head(keys), current);
			read_key = true;
			break;

		default:
			g_assert_not_reached();
		}

done:
		yaml_event_delete(&event);
	}

out:
	yaml_parser_delete(&parser);
	g_queue_free_full(keys, g_free);
	g_queue_free(containers);
	return (current);
}

static struct rpc_serializer yaml_serializer = {
	.name = "yaml",
	.serialize = rpc_yaml_serialize,
	.deserialize = rpc_yaml_deserialize
};

DECLARE_SERIALIZER(yaml_serializer);
