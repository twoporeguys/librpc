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
#include <inttypes.h>
#include <errno.h>
#include <glib.h>
#include <rpc/object.h>
#include <yaml.h>
#include "../linker_set.h"
#include "../internal.h"
#include "yaml.h"

static inline rpc_object
rpc_yaml_read_ext(yaml_parser_t *parser, const char *type)
{
	rpc_object_t ret = NULL;
	yaml_event_t event;
	bool read_key = true;
	int intval;
	const char *value;
	size_t length;
	char *key = NULL;
	char *endptr;
	int err_code = 0;
	char *err_msg = NULL;
	rpc_object_t err_extra = NULL;
	rpc_object_t err_stack = NULL;
#if defined(__linux__)
	int shmem_fd = 0;
	off_t shmem_addr = 0;
	size_t shmem_size = 0;
#endif

	for (;;) {
		if (!yaml_parser_parse(parser, &event)) {
			rpc_set_last_error(EINVAL, parser->problem, NULL);
			return (NULL);
		}

		switch (event.type) {
		case YAML_SCALAR_EVENT:
			value = (const char *)event.data.scalar.value;
			length = event.data.scalar.length;
			if (read_key) {
				g_free(key);
				key = g_strndup(value, length);

				read_key = false;
				break;
			}

			read_key = true;

			if (!g_strcmp0(key, YAML_ERROR_MSG)) {
				err_msg = g_strndup(value, length);
				break;
			}

			if (!g_strcmp0(key, YAML_ERROR_EXTRA)) {
				err_extra = rpc_yaml_deserialize(value,
				    length);
				break;
			}

			if (!g_strcmp0(key, YAML_ERROR_STACK)) {
				err_stack = rpc_string_create_len(value,
				    length);
				break;
			}

			intval = (int)g_ascii_strtoll(value,
			    &endptr, 10);

			if (!g_strcmp0(key, YAML_ERROR_CODE)) {
				err_code = intval;
				break;
			}

#if defined(__linux__)
			if (!g_strcmp0(key, YAML_SHMEM_ADDR)) {
				shmem_addr = (off_t)intval;
				break;
			}

			if (!g_strcmp0(key, YAML_SHMEM_LEN)) {
				shmem_size = (size_t)intval;
				break;
			}

			if (!g_strcmp0(key, YAML_SHMEM_FD)) {
				shmem_fd = intval;
				break;
			}
#endif

		case YAML_MAPPING_END_EVENT:
			goto done;

		default:
			break;
		}

		yaml_event_delete(&event);
	}


done:	yaml_event_delete(&event);
	g_free(key);

	if (!g_strcmp0(type, YAML_TAG_ERROR))
		ret = rpc_error_create_with_stack(err_code, err_msg, err_extra,
		    err_stack);

	g_free(err_msg);

#if defined(__linux__)

	if (!g_strcmp0(type, YAML_TAG_SHMEM))
		ret = rpc_shmem_recreate(shmem_fd, shmem_addr, shmem_size);
#endif

	return (ret);
}

static rpc_object_t
rpc_yaml_read_scalar(yaml_event_t *event)
{
	char *value = (char *)event->data.scalar.value;
	size_t len = event->data.scalar.length;
	char *tag = (char *)event->data.scalar.tag;
	int64_t intval;
	rpc_object_t ret;
	double doubleval;
	char *endptr;
	void *data_buf;
	size_t data_len;

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
		if (!g_strcmp0(tag, YAML_TAG_UINT64)) {
			ret = rpc_uint64_create((uint64_t)intval);
			goto done;
		}

		if (!g_strcmp0(tag, YAML_TAG_DATE)) {
			ret = rpc_date_create(intval);
			goto done;
		}

		if (!g_strcmp0(tag, YAML_TAG_FD)) {
			ret = rpc_fd_create((int)intval);
			goto done;
		}

		ret = rpc_int64_create(intval);
		goto done;
	}

	doubleval = (double)g_ascii_strtod(value, &endptr);
	if (*endptr == '\0') {
		ret = rpc_double_create(doubleval);
		goto done;
	}

	if (!g_strcmp0(tag, YAML_TAG_BINARY)) {
		data_buf = g_base64_decode(value, &data_len);
		ret = rpc_data_create(data_buf, data_len,
		    RPC_BINARY_DESTRUCTOR(g_free));
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

static inline int
rpc_yaml_write_kv(yaml_emitter_t *emitter, const char *key, const char *value)
{
	yaml_event_t event;
	int status;

	status = yaml_scalar_event_initialize(&event, NULL, NULL,
	    (yaml_char_t *)key, (int)strlen(key), 1, 1, YAML_ANY_SCALAR_STYLE);
	if (status != 1)
		return (status);
	status = yaml_emitter_emit(emitter, &event);
	if (status != 1)
		return (status);

	status = yaml_scalar_event_initialize(&event, NULL, NULL,
	    (yaml_char_t *)value, (int)strlen(value), 1, 1,
	    YAML_ANY_SCALAR_STYLE);
	if (status != 1)
		return (status);
	status = yaml_emitter_emit(emitter, &event);

	return (status);
}

static int
rpc_yaml_write_object(yaml_emitter_t *emitter, rpc_object_t object)
{
	__block yaml_event_t event;
	char *key;
	char *value;
	char *tag;
	__block int status;

	switch (rpc_get_type(object)) {
	case RPC_TYPE_NULL:
		value = "null";
		status = yaml_scalar_event_initialize(&event, NULL, NULL,
		    (yaml_char_t *)value, (int)strlen(value), 1, 1,
		    YAML_ANY_SCALAR_STYLE);
		break;

	case RPC_TYPE_BOOL:
		if (rpc_bool_get_value(object))
			value = "true";
		else
			value = "false";

		status = yaml_scalar_event_initialize(&event, NULL, NULL,
		    (yaml_char_t *)value, (int)strlen(value), 1, 1,
		    YAML_ANY_SCALAR_STYLE);
		break;

	case RPC_TYPE_INT64:
		value = g_strdup_printf("%" PRId64, rpc_int64_get_value(object));
		status = yaml_scalar_event_initialize(&event, NULL, NULL,
		    (yaml_char_t *)value, (int)strlen(value), 1, 1,
		    YAML_ANY_SCALAR_STYLE);
		g_free(value);
		break;

	case RPC_TYPE_UINT64:
		value = g_strdup_printf("%" PRIu64, rpc_uint64_get_value(object));
		tag = YAML_TAG_UINT64;
		status = yaml_scalar_event_initialize(&event, NULL,
		    (yaml_char_t *)tag, (yaml_char_t *)value,
		    (int)strlen(value), 0, 0, YAML_ANY_SCALAR_STYLE);
		g_free(value);
		break;

	case RPC_TYPE_FD:
		value = g_strdup_printf("%i", rpc_fd_get_value(object));
		tag = YAML_TAG_FD;
		status = yaml_scalar_event_initialize(&event, NULL,
		    (yaml_char_t *)tag, (yaml_char_t *)value,
		    (int)strlen(value), 0, 0, YAML_ANY_SCALAR_STYLE);
		g_free(value);
		break;

	case RPC_TYPE_DOUBLE:
		value = g_strdup_printf("%f", rpc_double_get_value(object));
		status = yaml_scalar_event_initialize(&event, NULL, NULL,
		    (yaml_char_t *)value, (int)strlen(value), 1, 1,
		    YAML_ANY_SCALAR_STYLE);
		g_free(value);
		break;

	case RPC_TYPE_DATE:
		value = g_strdup_printf("%" PRIu64, rpc_date_get_value(object));
		tag = YAML_TAG_DATE;
		status = yaml_scalar_event_initialize(&event, NULL,
		    (yaml_char_t *)tag, (yaml_char_t *)value,
		    (int)strlen(value), 0, 0, YAML_ANY_SCALAR_STYLE);
		g_free(value);
		break;

	case RPC_TYPE_STRING:
		status = yaml_scalar_event_initialize(&event, NULL, NULL,
		    (yaml_char_t *)rpc_string_get_string_ptr(object),
		    (int)rpc_string_get_length(object), 1, 1,
		    YAML_ANY_SCALAR_STYLE);
		break;

	case RPC_TYPE_BINARY:
		value = g_base64_encode(rpc_data_get_bytes_ptr(object),
		    rpc_data_get_length(object));
		tag = YAML_TAG_BINARY;
		status = yaml_scalar_event_initialize(&event, NULL,
		    (yaml_char_t *)tag, (yaml_char_t *)value,
		    (int)strlen(value), 0, 0, YAML_ANY_SCALAR_STYLE);
		g_free(value);
		break;

	case RPC_TYPE_ERROR:
		tag = YAML_TAG_ERROR;
		status = yaml_mapping_start_event_initialize(&event, NULL,
		    (yaml_char_t *)tag, 0, YAML_ANY_MAPPING_STYLE);
		if (status != 1)
			break;
		status = yaml_emitter_emit(emitter, &event);
		if (status != 1)
			break;

		key = YAML_ERROR_CODE;
		value = g_strdup_printf("%i", rpc_error_get_code(object));
		status = rpc_yaml_write_kv(emitter, key, value);
		g_free(value);
		if (status != 1)
			break;

		key = YAML_ERROR_MSG;
		status = rpc_yaml_write_kv(emitter, key,
		    rpc_error_get_message(object));
		if (status != 1)
			break;

		if (rpc_error_get_stack(object) != NULL) {
			key = YAML_ERROR_STACK;
			status = rpc_yaml_write_kv(emitter, key,
			    rpc_string_get_string_ptr(
			    rpc_error_get_stack(object)));
			if (status != 1)
				break;
		}

		if (rpc_error_get_extra(object) != NULL) {
			key = YAML_ERROR_EXTRA;
			status = yaml_scalar_event_initialize(&event, NULL,
			    NULL, (yaml_char_t *) key, (int)strlen(key), 1, 1,
			    YAML_ANY_SCALAR_STYLE);
			if (status != 1)
				break;
			status = yaml_emitter_emit(emitter, &event);
			if (status != 1)
				break;

			status = rpc_yaml_write_object(emitter, object);
			if (status != 1)
				break;
		}

		status = yaml_mapping_end_event_initialize(&event);
		break;

#if defined(__linux__)
	case RPC_TYPE_SHMEM:
		tag = YAML_TAG_SHMEM;
		status = yaml_mapping_start_event_initialize(&event, NULL,
		    (yaml_char_t *)tag, 0, YAML_ANY_MAPPING_STYLE);
		if (status != 1)
			break;
		status = yaml_emitter_emit(emitter, &event);
		if (status != 1)
			break;

		key = YAML_SHMEM_ADDR;
		value = g_strdup_printf("%li", rpc_shmem_get_offset(object));
		status = rpc_yaml_write_kv(emitter, key, value);
		g_free(value);
		if (status != 1)
			break;

		key = YAML_SHMEM_LEN;
		value = g_strdup_printf("%li", rpc_shmem_get_size(object));
		status = rpc_yaml_write_kv(emitter, key, value);
		g_free(value);
		if (status != 1)
			break;

		key = YAML_SHMEM_FD;
		value = g_strdup_printf("%i", rpc_shmem_get_fd(object));
		status = rpc_yaml_write_kv(emitter, key, value);
		g_free(value);
		if (status != 1)
			break;

		status = yaml_mapping_end_event_initialize(&event);
		break;

#endif
	case RPC_TYPE_DICTIONARY:
		status = yaml_mapping_start_event_initialize(&event, NULL, NULL,
		    1, YAML_ANY_MAPPING_STYLE);
		if (status != 1)
			break;
		status = yaml_emitter_emit(emitter, &event);
		if (status != 1)
			break;

		rpc_dictionary_apply(object, ^(const char *k, rpc_object_t v) {
			status = yaml_scalar_event_initialize(&event, NULL,
			    NULL, (yaml_char_t *)k, (int)strlen(k), 1, 1,
			    YAML_ANY_SCALAR_STYLE);
			if (status != 1)
				return ((bool)false);

			status = yaml_emitter_emit(emitter, &event);
			if (status != 1)
				return ((bool)false);

			status = rpc_yaml_write_object(emitter, v);
			if (status != 1)
				return ((bool)false);

			return ((bool)true);
		});
		if (status != 1)
			break;

		status = yaml_mapping_end_event_initialize(&event);
		break;

	case RPC_TYPE_ARRAY:
		status = yaml_sequence_start_event_initialize(&event, NULL, NULL,
		    1, YAML_ANY_SEQUENCE_STYLE);
		if (status != 1)
			break;
		status = yaml_emitter_emit(emitter, &event);
		if (status != 1)
			break;

		rpc_array_apply(object, ^(size_t idx __unused, rpc_object_t v) {
				status = rpc_yaml_write_object(emitter, v);
				if (status != 1)
					return ((bool)false);

				return ((bool)true);
		});
		if (status != 1)
			break;

		status = yaml_sequence_end_event_initialize(&event);
		break;

	default:
		g_assert_not_reached();
	}

	if (status == 0)
		return (status);

	return (yaml_emitter_emit(emitter, &event));
}

static int
rpc_yaml_write_output(void *data, unsigned char *buffer, size_t size)
{

	g_string_append_len((GString *)data, (const char *)buffer, size);
	return (1);
}

int
rpc_yaml_serialize(rpc_object_t obj, void **frame, size_t *size)
{
	yaml_emitter_t emitter;
	yaml_event_t event;
	GString *out_buffer;
	int status = 0;

	out_buffer = g_string_new(NULL);
	yaml_emitter_initialize(&emitter);
	yaml_emitter_set_canonical(&emitter, 0);
	yaml_emitter_set_encoding(&emitter, YAML_UTF8_ENCODING);
	yaml_emitter_set_unicode(&emitter, 1);
	yaml_emitter_set_indent(&emitter, 2);
	yaml_emitter_set_width(&emitter, 120);
	yaml_emitter_set_output(&emitter, &rpc_yaml_write_output,
	    (void *)out_buffer);
	yaml_emitter_open(&emitter);

	status = yaml_document_start_event_initialize(&event, NULL, NULL, NULL,
	    0);
	if (status != 1)
		goto done;
	status = yaml_emitter_emit(&emitter, &event);
	if (status != 1)
		goto done;

	status = rpc_yaml_write_object(&emitter, obj);
	if (status != 1)
		goto done;

	status = yaml_document_end_event_initialize(&event, 1);
	if (status != 1)
		goto done;

	status = yaml_emitter_emit(&emitter, &event);
	if (status != 1)
		goto done;

	status = yaml_emitter_close(&emitter);

done:	yaml_emitter_delete(&emitter);

	if (status != 1)
		g_string_free(out_buffer, true);
	else {
		*size = out_buffer->len;
		*frame = g_string_free(out_buffer, false);
	}

	return (status == 1 ? 0 : -1);
}

rpc_object_t
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
	char *tag;

	yaml_parser_initialize(&parser);
	yaml_parser_set_input_string(&parser, frame, size);

	while (!done) {
		if (!yaml_parser_parse(&parser, &event)) {
			rpc_set_last_error(EINVAL, parser.problem, NULL);
			goto out;
		}

		switch (event.type) {
			case YAML_MAPPING_START_EVENT:
				read_key = true;
				tag = (char *)event.data.mapping_start.tag;
				if (tag != NULL) {
					current = rpc_yaml_read_ext(&parser,
					    tag);
					if (current == NULL)
						goto out;
					break;
				}
				g_queue_push_head(containers,
				    rpc_dictionary_create());
				goto done;

			case YAML_MAPPING_END_EVENT:
				current = g_queue_pop_head(containers);
				break;

			case YAML_SEQUENCE_START_EVENT:
				g_queue_push_head(containers,
				    rpc_array_create());
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
			key = g_queue_pop_head(keys);
			rpc_dictionary_set_value(container, key, current);
			g_free(key);
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
