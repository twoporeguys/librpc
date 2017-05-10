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

#include <rpc/object.h>
#include "../../contrib/mpack/mpack.h"
#include "../internal.h"
#include "msgpack.h"

static int
rpc_msgpack_write_object(mpack_writer_t *writer, rpc_object_t object)
{
	int64_t timestamp;

	switch (object->ro_type) {
	case RPC_TYPE_NULL:
		mpack_write_nil(writer);
		break;

	case RPC_TYPE_BOOL:
		mpack_write_bool(writer, object->ro_value.rv_b);
		break;

	case RPC_TYPE_INT64:
		mpack_write_int(writer, object->ro_value.rv_i);
		break;

	case RPC_TYPE_UINT64:
		mpack_write_uint(writer, object->ro_value.rv_ui);
		break;

	case RPC_TYPE_DATE:
		timestamp = g_date_time_to_unix(object->ro_value.rv_datetime);
		mpack_write_ext(writer, MSGPACK_EXTTYPE_DATE,
		    (const char *)&timestamp, sizeof(int64_t));
		break;

	case RPC_TYPE_DOUBLE:
		mpack_write_double(writer, object->ro_value.rv_d);
		break;

	case RPC_TYPE_STRING:
		mpack_write_cstr(writer, object->ro_value.rv_str->str);
		break;

	case RPC_TYPE_BINARY:
		mpack_write_bin(writer, (char *)object->ro_value.rv_ptr,
		    (uint32_t)object->ro_size);
		break;

	case RPC_TYPE_FD:
		mpack_write_ext(writer, MSGPACK_EXTTYPE_FD,
		    (const char *)&object->ro_value.rv_fd,
		    sizeof(object->ro_value.rv_fd));
		break;

	case RPC_TYPE_DICTIONARY:
		mpack_start_map(writer, (uint32_t)rpc_dictionary_get_count(object));
		rpc_dictionary_apply(object, ^(const char *k, rpc_object_t v) {
		    mpack_write_cstr(writer, k);
		    rpc_msgpack_write_object(writer, v);
		    return ((bool)true);
		});
		mpack_finish_map(writer);
		break;

	case RPC_TYPE_ARRAY:
		mpack_start_array(writer, (uint32_t)rpc_array_get_count(object));
		rpc_array_apply(object, ^(size_t idx, rpc_object_t v) {
		    rpc_msgpack_write_object(writer, v);
		    return ((bool)true);
		});
		mpack_finish_array(writer);
		break;
	}

	return (0);
}

static rpc_object_t
rpc_msgpack_read_object(mpack_node_t node)
{
	__block int i;
	__block char *cstr;
	__block mpack_node_t tmp;
	__block rpc_object_t result;

	switch (mpack_node_type(node)) {
	case mpack_type_int:
		return (rpc_int64_create(mpack_node_i64(node)));

	case mpack_type_uint:
		return (rpc_uint64_create(mpack_node_u64(node)));

	case mpack_type_bool:
		return (rpc_bool_create(mpack_node_bool(node)));

	case mpack_type_double:
		return (rpc_double_create(mpack_node_double(node)));

	case mpack_type_str:
		cstr = g_strndup(mpack_node_str(node), mpack_node_strlen(node));
		return (rpc_string_create(cstr));

	case mpack_type_bin:
		return (rpc_data_create(mpack_node_data(node),
		    mpack_node_data_len(node), false));

	case mpack_type_array:
		result = rpc_array_create();
		for (i = 0; i < mpack_node_array_length(node); i++) {
			rpc_array_append_value(result, rpc_msgpack_read_object(
			    mpack_node_array_at(node, (uint32_t)i)));
		}
		return (result);

	case mpack_type_map:
		result = rpc_dictionary_create();
		for (i = 0; i < mpack_node_map_count(node); i++) {
			tmp = mpack_node_map_key_at(node, (uint32_t)i);
			cstr = g_strndup(mpack_node_str(tmp), mpack_node_strlen(tmp));
			rpc_dictionary_set_value(result, cstr,
			    rpc_msgpack_read_object(mpack_node_map_value_at(
				node, (uint32_t)i)));
		}
		return (result);

	case mpack_type_ext:
		break;

	case mpack_type_nil:
	default:
		return (rpc_null_create());
	}

	return (NULL);
}

int
rpc_msgpack_serialize(rpc_object_t obj, void **frame, size_t *size)
{
	mpack_writer_t writer;

	mpack_writer_init_growable(&writer, (char **)frame, size);
	rpc_msgpack_write_object(&writer, obj);
	mpack_writer_destroy(&writer);
	return (0);
}

rpc_object_t
rpc_msgpack_deserialize(const void *frame, size_t size)
{
	mpack_tree_t tree;
	rpc_object_t result;

	mpack_tree_init(&tree, frame, size);
	result = rpc_msgpack_read_object(mpack_tree_root(&tree));
	mpack_tree_destroy(&tree);

	return (result);
}
