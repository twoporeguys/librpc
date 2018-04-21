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

#include <glib.h>
#include <string.h>
#include <errno.h>
#include <rpc/object.h>
#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>
#include "../linker_set.h"
#include "../internal.h"
#include "json.h"

struct parse_context
{
	rpc_object_t result;
	GQueue *leaf_stack;
	char *key_buf;
};

static yajl_gen_status rpc_json_write_object(yajl_gen gen, rpc_object_t object);

static inline yajl_gen_status
rpc_json_write_ext_long(yajl_gen gen, const unsigned char *key, long value)
{
	yajl_gen_status status;

	status = yajl_gen_string(gen, key, strlen((const char *)key));
	if (status != yajl_gen_status_ok)
		return (status);

	status = yajl_gen_integer(gen, value);
	return (status);
}

static inline yajl_gen_status
rpc_json_write_ext_str(yajl_gen gen, const unsigned char *key,
    const char *value)
{
	yajl_gen_status status;

	status = yajl_gen_string(gen, key, strlen((const char *)key));
	if (status != yajl_gen_status_ok)
		return (status);

	status = yajl_gen_string(gen, (const unsigned char *)value,
	    strlen(value));
	return (status);
}

static inline yajl_gen_status
rpc_json_write_ext_obj(yajl_gen gen, const unsigned char *key,
    rpc_object_t value)
{
	yajl_gen_status status;

	status = yajl_gen_string(gen, key, strlen((const char *)key));
	if (status != yajl_gen_status_ok)
		return (status);

	status = rpc_json_write_object(gen, value);

	return (status);
}

#define rpc_json_write_ext(gen, key, value)		\
	_Generic(((void)0, (void)0, value),		\
		int64_t: rpc_json_write_ext_long,	\
		const char *: rpc_json_write_ext_str,	\
		rpc_object_t: rpc_json_write_ext_obj	\
	)(gen, key, value)

static int
rpc_json_context_insert_value(void *ctx_ptr, rpc_object_t value)
{
	rpc_object_t leaf;
	struct parse_context *ctx = (struct parse_context *)ctx_ptr;

	if (ctx->result == NULL) {
		ctx->result = value;
		g_queue_push_head(ctx->leaf_stack, value);
		return (1);
	}

	leaf = (rpc_object_t)g_queue_peek_head(ctx->leaf_stack);
	if (leaf == NULL)
		return (0);

	switch (leaf->ro_type) {
	case RPC_TYPE_ARRAY:
		if (ctx->key_buf != NULL)
			return (0);

		rpc_array_append_stolen_value(leaf, value);
		break;

	case RPC_TYPE_DICTIONARY:
		if (ctx->key_buf == NULL)
			return (0);

		rpc_dictionary_steal_value_internal(leaf, ctx->key_buf, value);
		g_free(ctx->key_buf);
		ctx->key_buf = NULL;
		break;

	default:
		return (0);
	}

	switch (value->ro_type) {
		case RPC_TYPE_DICTIONARY:
		case RPC_TYPE_ARRAY:
			g_queue_push_head(ctx->leaf_stack, value);

		default:
			break;
	}

	return (1);
}

static void
rpc_json_try_unpack_ext(void *ctx_ptr, rpc_object_t leaf)
{
	struct parse_context *ctx = ctx_ptr;
	rpc_object_t branch;
	rpc_object_t dict_value;
	rpc_object_t unpacked_value;
	void *data_buf;
	size_t data_len;
	const char *base64_data;
	GHashTableIter iter;
	gpointer key, value;
	int err_code;
	const char *err_msg;
	rpc_object_t err_extra;
	rpc_object_t err_stack;
#if defined(__linux__)
	int shmem_fd;
	off_t shmem_addr;
	size_t shmem_size;
#endif

	if (rpc_dictionary_has_key(leaf, JSON_EXTTYPE_UINT64)) {
		dict_value = rpc_dictionary_get_value(leaf,
		    JSON_EXTTYPE_UINT64);
		unpacked_value = rpc_uint64_create(
		    (uint64_t)rpc_int64_get_value(dict_value));

	} else if (rpc_dictionary_has_key(leaf, JSON_EXTTYPE_BINARY)) {
		dict_value = rpc_dictionary_get_value(leaf,
		    JSON_EXTTYPE_BINARY);
		base64_data = rpc_string_get_string_ptr(dict_value);
		data_buf = g_base64_decode(base64_data, &data_len);
		unpacked_value = rpc_data_create(data_buf, data_len,
		    RPC_BINARY_DESTRUCTOR(g_free));

	} else if (rpc_dictionary_has_key(leaf, JSON_EXTTYPE_DATE)) {
		dict_value = rpc_dictionary_get_value(leaf,
		    JSON_EXTTYPE_DATE);
		unpacked_value = rpc_date_create(
		    (int64_t)rpc_int64_get_value(dict_value));

	} else if (rpc_dictionary_has_key(leaf, JSON_EXTTYPE_FD)) {
		dict_value = rpc_dictionary_get_value(leaf,
		    JSON_EXTTYPE_FD);
		unpacked_value = rpc_fd_create(
		    (int)rpc_int64_get_value(dict_value));

	} else if (rpc_dictionary_has_key(leaf, JSON_EXTTYPE_ERROR)) {
		dict_value = rpc_dictionary_get_value(leaf,
		    JSON_EXTTYPE_ERROR);
		err_code = (int)rpc_dictionary_get_int64(dict_value,
		    JSON_EXTTYPE_ERROR_CODE);
		err_msg = rpc_dictionary_get_string(dict_value,
		    JSON_EXTTYPE_ERROR_MSG);
		err_extra = rpc_dictionary_get_value(dict_value,
		    JSON_EXTTYPE_ERROR_XTRA);
		err_stack = rpc_dictionary_get_value(dict_value,
		    JSON_EXTTYPE_ERROR_STCK);
		unpacked_value = rpc_error_create_with_stack(err_code, err_msg,
		    err_extra, err_stack);

#if defined(__linux__)
	} else if (rpc_dictionary_has_key(leaf, JSON_EXTTYPE_SHMEM)) {
		dict_value = rpc_dictionary_get_value(leaf,
		    JSON_EXTTYPE_SHMEM);

		shmem_fd = (int)rpc_dictionary_get_int64(dict_value,
		    JSON_EXTTYPE_SHMEM_FD);
		shmem_addr = rpc_dictionary_get_int64(dict_value,
		    JSON_EXTTYPE_SHMEM_ADDR);
		shmem_size = (size_t)rpc_dictionary_get_int64(dict_value,
		    JSON_EXTTYPE_SHMEM_LEN);

		unpacked_value = rpc_shmem_recreate(shmem_fd, shmem_addr,
		    shmem_size);

#endif
	} else
		return;

	branch = (rpc_object_t)g_queue_peek_head(ctx->leaf_stack);

	if (branch == NULL) {
		ctx->result = unpacked_value;
		g_queue_push_head(ctx->leaf_stack, unpacked_value);
		rpc_release(leaf);

	} else if (branch->ro_type == RPC_TYPE_DICTIONARY) {
		g_hash_table_iter_init(&iter, branch->ro_value.rv_dict);

		while (g_hash_table_iter_next(&iter, &key, &value)) {
			if (value == leaf) {
				g_hash_table_iter_replace(&iter,
				    unpacked_value);
				break;
			}
		}

	} else if (branch->ro_type == RPC_TYPE_ARRAY) {
		rpc_array_steal_value(branch, (rpc_array_get_count(branch) - 1),
		    unpacked_value);

	}
}


static int
rpc_json_parse_null(void *ctx)
{

	return rpc_json_context_insert_value(ctx, rpc_null_create());
}

static int
rpc_json_parse_boolean(void * ctx, int value)
{

	return rpc_json_context_insert_value(ctx, rpc_bool_create((bool)value));
}

static int
rpc_json_parse_integer(void *ctx, long long value)
{

	return rpc_json_context_insert_value(ctx,
	    rpc_int64_create((int64_t)value));
}

static int
rpc_json_parse_double(void *ctx, double value)
{

	return rpc_json_context_insert_value(ctx, rpc_double_create(value));
}

static int
rpc_json_parse_string(void *ctx, const unsigned char *value, size_t len)
{

	return rpc_json_context_insert_value(ctx,
	    rpc_string_create_len((const char *)value, len));
}

static int
rpc_json_map_key(void *ctx_ptr, const unsigned char *key, size_t key_len)
{
	struct parse_context *ctx = ctx_ptr;
	GString *key_copy;

	if (ctx->key_buf != NULL)
		return (0);

	key_copy = g_string_new_len((const char *)key, key_len);
	ctx->key_buf = g_string_free(key_copy, false);
	return 1;
}

static int
rpc_json_start_map(void *ctx)
{

	return rpc_json_context_insert_value(ctx, rpc_dictionary_create());
}


static int
rpc_json_end_map(void *ctx_ptr)
{
	struct parse_context *ctx = ctx_ptr;
	rpc_object_t leaf;
	size_t leaf_size;

	leaf = (rpc_object_t)g_queue_pop_head(ctx->leaf_stack);

	if (leaf == NULL)
		return (0);

	if (leaf->ro_type != RPC_TYPE_DICTIONARY)
		return (0);

	if (ctx->key_buf != NULL)
		return (0);

	leaf_size = rpc_dictionary_get_count(leaf);
	if (leaf_size == 1)
		rpc_json_try_unpack_ext(ctx_ptr, leaf);

	return (1);
}

static int
rpc_json_start_array(void *ctx)
{

	return rpc_json_context_insert_value(ctx, rpc_array_create());
}

static int
rpc_json_end_array(void *ctx_ptr)
{
	struct parse_context *ctx = ctx_ptr;
	rpc_object_t leaf = (rpc_object_t)g_queue_pop_head(ctx->leaf_stack);

	if (leaf == NULL)
		return (0);

	if (leaf->ro_type != RPC_TYPE_ARRAY)
		return (0);

	if (ctx->key_buf != NULL)
		return (0);

	return (1);
}

static yajl_callbacks callbacks = {
    rpc_json_parse_null,
    rpc_json_parse_boolean,
    rpc_json_parse_integer,
    rpc_json_parse_double,
    NULL,
    rpc_json_parse_string,
    rpc_json_start_map,
    rpc_json_map_key,
    rpc_json_end_map,
    rpc_json_start_array,
    rpc_json_end_array
};

static yajl_gen_status
rpc_json_write_object_ext(yajl_gen gen, rpc_object_t object,
    const uint8_t *type, size_t type_size)
{
	yajl_gen_status status;
	const void *data_buf;
	char *base64_data;

	if ((status = yajl_gen_map_open(gen)) != yajl_gen_status_ok)
		return (status);

	status = yajl_gen_string(gen, type, type_size);
	if (status != yajl_gen_status_ok)
		return (status);

	switch (object->ro_type) {
	case RPC_TYPE_UINT64:
		status = yajl_gen_integer(gen,
		    (long long)rpc_uint64_get_value(object));
		break;

	case RPC_TYPE_DATE:
		status = yajl_gen_integer(gen, rpc_date_get_value(object));
		break;

	case RPC_TYPE_FD:
		status = yajl_gen_integer(gen, rpc_fd_get_value(object));
		break;

	case RPC_TYPE_BINARY:
		data_buf = rpc_data_get_bytes_ptr(object);
		base64_data = g_base64_encode((const guchar *)data_buf,
		    object->ro_value.rv_bin.rbv_length);

		status = yajl_gen_string(gen, (const uint8_t *)base64_data,
		    strlen(base64_data));
		g_free(base64_data);
		break;

	case RPC_TYPE_ERROR:
		if ((status = yajl_gen_map_open(gen)) != yajl_gen_status_ok)
			return (status);

		status = rpc_json_write_ext(gen,
		    (const uint8_t *)JSON_EXTTYPE_ERROR_CODE,
		    (int64_t)rpc_error_get_code(object));
		if (status != yajl_gen_status_ok)
			return (status);

		status = rpc_json_write_ext(gen,
		    (const uint8_t *)JSON_EXTTYPE_ERROR_MSG,
		    rpc_error_get_message(object));
		if (status != yajl_gen_status_ok)
			return (status);

		if (rpc_error_get_extra(object) != NULL) {
			status = rpc_json_write_ext(gen,
			    (const uint8_t *) JSON_EXTTYPE_ERROR_XTRA,
			    rpc_error_get_extra(object));
			if (status != yajl_gen_status_ok)
				return (status);
		}

		if (rpc_error_get_stack(object) != NULL) {
			status = rpc_json_write_ext(gen,
			    (const uint8_t *) JSON_EXTTYPE_ERROR_STCK,
			    rpc_error_get_stack(object));
			if (status != yajl_gen_status_ok)
				return (status);
		}

		status = yajl_gen_map_close(gen);
		break;

#if defined(__linux__)
	case RPC_TYPE_SHMEM:
		if ((status = yajl_gen_map_open(gen)) != yajl_gen_status_ok)
			return (status);

		status = rpc_json_write_ext(gen,
		    (const uint8_t *)JSON_EXTTYPE_SHMEM_ADDR,
		    (int64_t)rpc_shmem_get_offset(object));
		if (status != yajl_gen_status_ok)
			return (status);

		status = rpc_json_write_ext(gen,
		    (const uint8_t *)JSON_EXTTYPE_SHMEM_LEN,
		    (int64_t)rpc_shmem_get_size(object));
		if (status != yajl_gen_status_ok)
			return (status);

		status = rpc_json_write_ext(gen,
		    (const uint8_t *)JSON_EXTTYPE_SHMEM_FD,
		    (int64_t)rpc_shmem_get_fd(object));
		if (status != yajl_gen_status_ok)
			return (status);

		status = yajl_gen_map_close(gen);
		break;
#endif

	default:
		break;

	}

	if (status != yajl_gen_status_ok)
		return (status);

	return (yajl_gen_map_close(gen));
}

static yajl_gen_status
rpc_json_write_object(yajl_gen gen, rpc_object_t object)
{
	__block yajl_gen_status status;

	switch (object->ro_type) {
	case RPC_TYPE_NULL:
		return (yajl_gen_null(gen));

	case RPC_TYPE_BOOL:
		return (yajl_gen_bool(gen, rpc_bool_get_value(object)));

	case RPC_TYPE_INT64:
		return (yajl_gen_integer(gen, rpc_int64_get_value(object)));

	case RPC_TYPE_UINT64:
		return (rpc_json_write_object_ext(gen, object,
		    (const uint8_t *)JSON_EXTTYPE_UINT64,
		    strlen(JSON_EXTTYPE_UINT64)));

	case RPC_TYPE_DATE:
		return (rpc_json_write_object_ext(gen, object,
		    (const uint8_t *)JSON_EXTTYPE_DATE,
		    strlen(JSON_EXTTYPE_DATE)));

	case RPC_TYPE_BINARY:
		return (rpc_json_write_object_ext(gen, object,
		    (const uint8_t *)JSON_EXTTYPE_BINARY,
		    strlen(JSON_EXTTYPE_BINARY)));

	case RPC_TYPE_FD:
		return (rpc_json_write_object_ext(gen, object,
		    (const uint8_t *)JSON_EXTTYPE_FD,
		    strlen(JSON_EXTTYPE_FD)));

	case RPC_TYPE_ERROR:
		return (rpc_json_write_object_ext(gen, object,
		    (const uint8_t *)JSON_EXTTYPE_ERROR,
		    strlen(JSON_EXTTYPE_ERROR)));

#if defined(__linux__)
	case RPC_TYPE_SHMEM:
		return (rpc_json_write_object_ext(gen, object,
		    (const uint8_t *)JSON_EXTTYPE_SHMEM,
		    strlen(JSON_EXTTYPE_SHMEM)));
#endif

	case RPC_TYPE_DOUBLE:
		return (yajl_gen_double(gen, rpc_double_get_value(object)));

	case RPC_TYPE_STRING:
		return (yajl_gen_string(gen,
		    (const uint8_t *)rpc_string_get_string_ptr(object),
		    rpc_string_get_length(object)));

	case RPC_TYPE_DICTIONARY:
		status = yajl_gen_map_open(gen);
		if (status != yajl_gen_status_ok)
			return (status);

		rpc_dictionary_apply(object, ^(const char *k, rpc_object_t v) {
			status = yajl_gen_string(gen, (const uint8_t *)k,
			    strlen(k));
			if (status != yajl_gen_status_ok)
				return ((bool)false);

			status = rpc_json_write_object(gen, v);
			if (status != yajl_gen_status_ok)
				return ((bool)false);

			return ((bool)true);
		});
		if (status != yajl_gen_status_ok)
			return (status);

		return (yajl_gen_map_close(gen));

	case RPC_TYPE_ARRAY:
		status = yajl_gen_array_open(gen);
		if (status != yajl_gen_status_ok)
			return (status);

		rpc_array_apply(object, ^(size_t idx __unused, rpc_object_t v) {
			status = rpc_json_write_object(gen, v);
			if (status != yajl_gen_status_ok)
				return ((bool)false);

			return ((bool)true);
		});
		if (status != yajl_gen_status_ok)
			return (status);

		return (yajl_gen_array_close(gen));
	}

	return (yajl_gen_status_ok);
}

int
rpc_json_serialize(rpc_object_t obj, void **frame, size_t *size)
{
	yajl_gen gen = yajl_gen_alloc(NULL);
	yajl_gen_status status;
	GString *out_buffer;

	out_buffer = g_string_new(NULL);
	yajl_gen_config(gen, yajl_gen_print_callback,
	    (yajl_print_t)g_string_append_len, (void *)out_buffer);

	if ((status = rpc_json_write_object(gen, obj)) != yajl_gen_status_ok)
		goto end;

	*size = out_buffer->len;

end:	yajl_gen_free(gen);
	*frame = g_string_free(out_buffer, false);
	return (status);
}

rpc_object_t
rpc_json_deserialize(const void *frame, size_t size)
{
	yajl_handle handle;
	struct parse_context ctx;

	ctx.result = NULL;
	ctx.leaf_stack = g_queue_new();
	ctx.key_buf = NULL;

	handle = yajl_alloc(&callbacks, NULL, (void *) &ctx);
	if (yajl_parse(handle, (const guchar *)frame, size) != yajl_status_ok) {
		rpc_release(ctx.result);
		rpc_set_last_error(EINVAL, "Parse error", NULL);
		goto end;
	}

	if (yajl_complete_parse(handle) != yajl_status_ok) {
		rpc_release(ctx.result);
		rpc_set_last_error(EINVAL, "Parse error", NULL);
		goto end;
	}

end:
	yajl_free(handle);
	g_queue_free(ctx.leaf_stack);
	g_free(ctx.key_buf);
	return (ctx.result);
}

static struct rpc_serializer json_serializer = {
	.name = "json",
	.serialize = &rpc_json_serialize,
	.deserialize = &rpc_json_deserialize
};

DECLARE_SERIALIZER(json_serializer);
