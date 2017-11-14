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

#include <assert.h>
#include <rpc/object.h>
#ifdef __APPLE__
#include "../endian.h"
#endif
#include "../../contrib/mpack/mpack.h"
#include "../linker_set.h"
#include "../internal.h"
#include "msgpack.h"

static void rpc_msgpack_write_error(mpack_writer_t *, rpc_object_t);
static rpc_object_t rpc_msgpack_read_error(mpack_tree_t *);
static int rpc_msgpack_write_object(mpack_writer_t *, rpc_object_t);
#if defined(__linux__)
static rpc_object_t rpc_msgpack_read_shmem(mpack_tree_t *);
static void rpc_msgpack_write_shmem(mpack_writer_t *, rpc_object_t);
#endif
static rpc_object_t rpc_msgpack_read_object(mpack_node_t);

static void
rpc_msgpack_write_error(mpack_writer_t *writer, rpc_object_t error)
{
	assert(rpc_get_type(error) == RPC_TYPE_ERROR);

	mpack_start_map(writer, 4);
	mpack_write_cstr(writer, MSGPACK_ERROR_CODE);
	mpack_write_i64(writer, rpc_error_get_code(error));
	mpack_write_cstr(writer, MSGPACK_ERROR_MESSAGE);
	mpack_write_cstr(writer, rpc_error_get_message(error));
	if (rpc_error_get_extra(error) != NULL) {
		mpack_write_cstr(writer, MSGPACK_ERROR_EXTRA);
		rpc_msgpack_write_object(writer, rpc_error_get_extra(error));
	}
	if (rpc_error_get_stack(error) != NULL) {
		mpack_write_cstr(writer, MSGPACK_ERROR_STACK);
		rpc_msgpack_write_object(writer, rpc_error_get_stack(error));
	}
}

static rpc_object_t
rpc_msgpack_read_error(mpack_tree_t *tree)
{
	mpack_node_t root;
	int code;
	char *msg;
	rpc_object_t extra;
	rpc_object_t stack;
	rpc_object_t result;

	root = mpack_tree_root(tree);
	code = (int)mpack_node_i64(mpack_node_map_cstr(root,
	    MSGPACK_ERROR_CODE));
	msg = mpack_node_cstr_alloc(mpack_node_map_cstr(root,
	    MSGPACK_ERROR_MESSAGE), 1024);
	extra = rpc_msgpack_read_object(mpack_node_map_cstr(root,
	    MSGPACK_ERROR_EXTRA));
	stack = rpc_msgpack_read_object(mpack_node_map_cstr(root,
	    MSGPACK_ERROR_STACK));
	result = rpc_error_create_with_stack((int)code, msg,
	    extra, stack);

	free(msg);
	return (result);
}

#if defined(__linux__)
static void
rpc_msgpack_write_shmem(mpack_writer_t *writer, rpc_object_t shmem)
{
	assert(rpc_get_type(shmem) == RPC_TYPE_SHMEM);

	mpack_start_map(writer, 3);
	mpack_write_cstr(writer, MSGPACK_SHMEM_FD);
	mpack_write_i64(writer, shmem->ro_value.rv_shmem.rsb_fd);
	mpack_write_cstr(writer, MSGPACK_SHMEM_OFFSET);
	mpack_write_u64(writer, shmem->ro_value.rv_shmem.rsb_offset);
	mpack_write_cstr(writer, MSGPACK_SHMEM_LEN);
	mpack_write_u64(writer, shmem->ro_value.rv_shmem.rsb_size);
}

static rpc_object_t
rpc_msgpack_read_shmem(mpack_tree_t *tree)
{
	mpack_node_t root;
	int fd;
	uint64_t offset, len;

	root = mpack_tree_root(tree);
	fd = (int)mpack_node_i64(mpack_node_map_cstr(root, MSGPACK_SHMEM_FD));
	offset = mpack_node_u64(mpack_node_map_cstr(root, MSGPACK_SHMEM_OFFSET));
	len = mpack_node_u64(mpack_node_map_cstr(root, MSGPACK_SHMEM_LEN));
	return (rpc_shmem_recreate(fd, (off_t)offset, (size_t)len));
}

#endif
static int
rpc_msgpack_write_object(mpack_writer_t *writer, rpc_object_t object)
{
	int64_t timestamp;
	mpack_writer_t subwriter;
	char *buffer;
	size_t len;
	struct {
		uint8_t tag;
		uint64_t value;
	} __attribute__((packed)) be_int64;
	struct {
		uint8_t tag;
		uint32_t value;
	} __attribute__((packed)) be_int32;

	switch (object->ro_type) {
	case RPC_TYPE_NULL:
		mpack_write_nil(writer);
		break;

	case RPC_TYPE_BOOL:
		mpack_write_bool(writer, object->ro_value.rv_b);
		break;

	case RPC_TYPE_INT64:
		if (object->ro_value.rv_i > (1L << 32)) {
			be_int64.value = htobe64(object->ro_value.rv_i);
			be_int64.tag = 0xd3;
			mpack_write_object_bytes(writer,
			    (const char *)&be_int64, sizeof(be_int64));
		} else {
			be_int32.value = htobe32(object->ro_value.rv_i);
			be_int32.tag = 0xd2;
			mpack_write_object_bytes(writer,
			    (const char *)&be_int32, sizeof(be_int32));
		}
		break;

	case RPC_TYPE_UINT64:
		if (object->ro_value.rv_i > (1L << 32)) {
			be_int64.value = htobe64(object->ro_value.rv_ui);
			be_int64.tag = 0xcf;
			mpack_write_object_bytes(writer,
			    (const char *)&be_int64, sizeof(be_int64));
		} else {
			be_int32.value = htobe32(object->ro_value.rv_ui);
			be_int32.tag = 0xd2;
			mpack_write_object_bytes(writer,
			    (const char *)&be_int32, sizeof(be_int32));
		}
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
		mpack_write_bin(writer, (char *)object->ro_value.rv_bin.rbv_ptr,
		    (uint32_t)object->ro_value.rv_bin.rbv_length);
		break;

	case RPC_TYPE_FD:
		mpack_write_ext(writer, MSGPACK_EXTTYPE_FD,
		    (const char *)&object->ro_value.rv_fd,
		    sizeof(object->ro_value.rv_fd));
		break;

#if defined(__linux__)
	case RPC_TYPE_SHMEM:
		mpack_writer_init_growable(&subwriter, &buffer, &len);
		rpc_msgpack_write_shmem(&subwriter, object);
		mpack_writer_destroy(&subwriter);
		mpack_write_ext(writer, MSGPACK_EXTTYPE_SHMEM,
		    buffer, len);
		break;
#endif

	case RPC_TYPE_ERROR:
		mpack_writer_init_growable(&subwriter, &buffer, &len);
		rpc_msgpack_write_error(&subwriter, object);
		mpack_writer_destroy(&subwriter);
		mpack_write_ext(writer, MSGPACK_EXTTYPE_ERROR,
		    (const char *)buffer, len);
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
		rpc_array_apply(object, ^(size_t idx __unused, rpc_object_t v) {
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
	int *fd;
	void *buffer;
	int64_t *date;
	mpack_tree_t subtree;
	__block size_t i;
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
		result = rpc_string_create(cstr);
		g_free(cstr);
		return (result);

	case mpack_type_bin:
		buffer = g_memdup(mpack_node_data(node), mpack_node_data_len(node));
		return (rpc_data_create(buffer, mpack_node_data_len(node),
		    RPC_BINARY_DESTRUCTOR(g_free)));

	case mpack_type_array:
		result = rpc_array_create();
		for (i = 0; i < mpack_node_array_length(node); i++) {
			rpc_array_append_stolen_value(result, rpc_msgpack_read_object(
			    mpack_node_array_at(node, (uint32_t)i)));
		}
		return (result);

	case mpack_type_map:
		result = rpc_dictionary_create();
		for (i = 0; i < mpack_node_map_count(node); i++) {
			tmp = mpack_node_map_key_at(node, (uint32_t)i);
			cstr = g_strndup(mpack_node_str(tmp), mpack_node_strlen(tmp));
			rpc_dictionary_steal_value(result, cstr,
			    rpc_msgpack_read_object(mpack_node_map_value_at(
				node, (uint32_t)i)));
			g_free(cstr);
		}
		return (result);

	case mpack_type_ext:
		switch (mpack_node_exttype(node)) {
		case MSGPACK_EXTTYPE_DATE:
			date = (int64_t *)mpack_node_data(node);
			return (rpc_date_create(*date));

		case MSGPACK_EXTTYPE_FD:
			fd = (int *)mpack_node_data(node);
			return (rpc_fd_create(*fd));

#if defined(__linux__)
		case MSGPACK_EXTTYPE_SHMEM:
			mpack_tree_init(&subtree, mpack_node_data(node),
			    mpack_node_data_len(node));
			result = rpc_msgpack_read_shmem(&subtree);
			mpack_tree_destroy(&subtree);
			return (result);
#endif

		case MSGPACK_EXTTYPE_ERROR:
			mpack_tree_init(&subtree, mpack_node_data(node),
			    mpack_node_data_len(node));
			result = rpc_msgpack_read_error(&subtree);
			mpack_tree_destroy(&subtree);
			return (result);

		default:
			return (rpc_null_create());
		}

	case mpack_type_nil:
	default:
		return (rpc_null_create());
	}
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

static struct rpc_serializer msgpack_serializer = {
	.name = "msgpack",
    	.serialize = &rpc_msgpack_serialize,
    	.deserialize = &rpc_msgpack_deserialize
};

DECLARE_SERIALIZER(msgpack_serializer);
