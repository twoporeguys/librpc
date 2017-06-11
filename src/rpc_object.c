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


#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <rpc/object.h>
#include "serializer/json.h"
#include "internal.h"
#if defined(__linux__)
#include "memfd.h"
#endif

GPrivate rpc_last_error = G_PRIVATE_INIT((GDestroyNotify)g_free);

static const char *rpc_types[] = {
    [RPC_TYPE_NULL] = "null",
    [RPC_TYPE_BOOL] = "bool",
    [RPC_TYPE_UINT64] = "uint64",
    [RPC_TYPE_INT64] = "int64",
    [RPC_TYPE_DOUBLE] = "double",
    [RPC_TYPE_DATE] = "date",
    [RPC_TYPE_STRING] = "string",
    [RPC_TYPE_BINARY] = "binary",
    [RPC_TYPE_FD] = "fd",
    [RPC_TYPE_DICTIONARY] = "dictionary",
#if defined(__linux__)
    [RPC_TYPE_SHMEM] = "shmem",
#endif
    [RPC_TYPE_ARRAY] = "array"
};

rpc_object_t
rpc_prim_create(rpc_type_t type, union rpc_value val)
{
	struct rpc_object *ro;

	ro = (rpc_object_t)g_malloc(sizeof(*ro));
	if (ro == NULL)
		abort();

	ro->ro_type = type;
	ro->ro_value = val;
	ro->ro_refcnt = 1;

	return (ro);
}

static size_t
rpc_data_hash(const uint8_t *data, size_t length)
{
	size_t hash = 5381;

	while (length--)
		hash = ((hash << 5) + hash) + data[length];

	return (hash);
}

static void
rpc_create_description(GString *description, rpc_object_t object,
    unsigned int indent_lvl, bool nested)
{
	unsigned int local_indent_lvl = indent_lvl + 1;
	size_t data_length, i;
	uint8_t *data_ptr;
	char *str_date;

	if ((indent_lvl > 0) && (!nested))
		g_string_append_printf(description, "%*s", (indent_lvl * 4),
		    "");

	g_string_append_printf(description, "<%s> ",
	    rpc_types[object->ro_type]);

	switch (object->ro_type) {
	case RPC_TYPE_NULL:
		break;

	case RPC_TYPE_BOOL:
		if (object->ro_value.rv_b == true)
			g_string_append(description, "true");
		else
			g_string_append(description, "false");

		break;

	case RPC_TYPE_INT64:
		g_string_append_printf(description, "%" PRId64 "",
		    object->ro_value.rv_i);
		break;

	case RPC_TYPE_FD:
		g_string_append_printf(description, "%u",
		    object->ro_value.rv_fd);
		break;

	case RPC_TYPE_UINT64:
		g_string_append_printf(description, "%" PRIu64 "",
		    object->ro_value.rv_ui);
		break;

	case RPC_TYPE_DOUBLE:
		g_string_append_printf(description, "%f",
		    object->ro_value.rv_d);
		break;

	case RPC_TYPE_DATE:
		str_date = g_date_time_format(object->ro_value.rv_datetime,
		    "%F %T");
		g_string_append(description, str_date);
		g_free(str_date);
		break;

	case RPC_TYPE_STRING:
		g_string_append_printf(description, "\"%s\"",
		    rpc_string_get_string_ptr(object));
		break;

	case RPC_TYPE_BINARY:
		data_ptr = (uint8_t *)rpc_data_get_bytes_ptr(object);
		data_length = MIN(object->ro_value.rv_bin.length, 16);

		for (i = 0; i < data_length; i++)
			g_string_append_printf(description, "%02x",
			    data_ptr[i]);

		if (data_length < object->ro_value.rv_bin.length)
			g_string_append(description, " ...");

		break;
#if defined(__linux__)
	case RPC_TYPE_SHMEM:
		g_string_append(description, "shared memory");
		break;
#endif
	case RPC_TYPE_DICTIONARY:
		g_string_append(description, "{\n");
		rpc_dictionary_apply(object, ^(const char *k, rpc_object_t v) {
			g_string_append_printf(description, "%*s%s: ",
			    (local_indent_lvl * 4), "", k);
			rpc_create_description(description, v, local_indent_lvl,
			    true);
			g_string_append(description, ",\n");
			return ((bool)true);
		});
		if (indent_lvl > 0)
			g_string_append_printf(description, "%*s",
			    (indent_lvl * 4), "");

		g_string_append(description, "}");
		break;

	case RPC_TYPE_ARRAY:
		g_string_append(description, "[\n");
		rpc_array_apply(object, ^(size_t idx, rpc_object_t v) {
			g_string_append_printf(
			    description, "%*s%u: ",
			    (local_indent_lvl * 4),
			    "",
			    (unsigned int)idx);

			rpc_create_description(description, v,
			    local_indent_lvl, true);
			g_string_append(description, ",\n");
			return ((bool)true);
		});
		if (indent_lvl > 0)
			g_string_append_printf(description, "%*s",
			    (indent_lvl * 4), "");

		g_string_append(description, "]");
		break;
	}

	if (!nested)
		g_string_append(description, "\n");
}

void
rpc_set_last_error(GError *g_error) {
	rpc_error_t error;

	error = g_malloc(sizeof(rpc_error_t));
	error->code = g_error->code;
	error->message = g_strdup(g_error->message);

	g_private_replace(&rpc_last_error, error);
}

rpc_error_t
rpc_get_last_error(void) {
	rpc_error_t error;

	if ((error = g_private_get(&rpc_last_error)) == NULL)
		return (NULL);

	return error;
}

inline rpc_object_t
rpc_object_from_json(const void *frame, size_t size)
{

	return rpc_json_deserialize(frame, size);
}

inline int
rpc_object_to_json(rpc_object_t object, void **frame, size_t *size)
{

	return rpc_json_serialize(object, frame, size);
}

inline rpc_object_t
rpc_retain(rpc_object_t object)
{

	g_atomic_int_inc(&object->ro_refcnt);
	return (object);
}

inline int
rpc_release_impl(rpc_object_t object)
{

	if (object == NULL)
		return (0);

	assert(object->ro_refcnt > 0);
	if (g_atomic_int_dec_and_test(&object->ro_refcnt)) {
		switch (object->ro_type) {
		case RPC_TYPE_BINARY:
			if (object->ro_value.rv_bin.copy == true)
				g_free((void *)object->ro_value.rv_bin.ptr);

			break;

		case RPC_TYPE_STRING:
			g_string_free(object->ro_value.rv_str, true);
			break;

		case RPC_TYPE_DATE:
			g_date_time_unref(object->ro_value.rv_datetime);
			break;

		case RPC_TYPE_ARRAY:
			g_ptr_array_unref(object->ro_value.rv_list);
			break;

		case RPC_TYPE_DICTIONARY:
			g_hash_table_unref(object->ro_value.rv_dict);
			break;
#if defined(__linux__)
		case RPC_TYPE_SHMEM:
			rpc_shmem_free(object->ro_value.rv_shmem);
#endif
		default:
			break;
		}
		g_free(object);
		return (0);
	}

	return (object->ro_refcnt);
}

inline rpc_type_t
rpc_get_type(rpc_object_t object)
{

	if (object == NULL)
		return (RPC_TYPE_NULL);

	return (object->ro_type);
}

inline rpc_object_t
rpc_copy(rpc_object_t object)
{
	rpc_object_t tmp;
#if defined(__linux__)
	rpc_shmem_block_t block;
#endif

	switch (object->ro_type) {
	case RPC_TYPE_NULL:
		return (rpc_null_create());

	case RPC_TYPE_BOOL:
		return (rpc_bool_create(object->ro_value.rv_b));

	case RPC_TYPE_INT64:
		return (rpc_int64_create(object->ro_value.rv_i));

	case RPC_TYPE_UINT64:
		return (rpc_uint64_create(object->ro_value.rv_ui));

	case RPC_TYPE_DATE:
		return (rpc_date_create(rpc_date_get_value(object)));

	case RPC_TYPE_DOUBLE:
		return (rpc_double_create(object->ro_value.rv_d));

	case RPC_TYPE_FD:
		return (rpc_fd_create(rpc_fd_dup(object)));

	case RPC_TYPE_STRING:
		return (rpc_string_create(rpc_string_get_string_ptr(object)));

	case RPC_TYPE_BINARY:
		return rpc_data_create(rpc_data_get_bytes_ptr(object),
		    rpc_data_get_length(object), true);

#if defined(__linux__)
	case RPC_TYPE_SHMEM:
		block = g_malloc(sizeof(*block));
		block->rsb_fd = dup(object->ro_value.rv_shmem->rsb_fd);
		block->rsb_offset = object->ro_value.rv_shmem->rsb_offset;
		block->rsb_size = object->ro_value.rv_shmem->rsb_size;
		return (rpc_shmem_create(block));
#endif

	case RPC_TYPE_DICTIONARY:
		tmp = rpc_dictionary_create();
		rpc_dictionary_apply(object, ^(const char *k, rpc_object_t v) {
		    	rpc_dictionary_steal_value(tmp, k, rpc_copy(v));
		    	return ((bool)true);
		});
		return (tmp);

	case RPC_TYPE_ARRAY:
		tmp = rpc_array_create();
		rpc_array_apply(object, ^(size_t idx, rpc_object_t v) {
			rpc_array_steal_value(tmp, idx, rpc_copy(v));
		    	return ((bool)true);
		});
		return (tmp);
	}

	return (NULL);
}

inline bool
rpc_equal(rpc_object_t o1, rpc_object_t o2)
{

	return (rpc_hash(o1) == rpc_hash(o2));
}

inline size_t
rpc_hash(rpc_object_t object)
{
	__block size_t hash = 0;

	switch (object->ro_type) {
	case RPC_TYPE_NULL:
		return (0);

	case RPC_TYPE_BOOL:
		return ((size_t)object->ro_value.rv_b);

	case RPC_TYPE_INT64:
		return ((size_t)object->ro_value.rv_i);

	case RPC_TYPE_UINT64:
		return ((size_t)object->ro_value.rv_ui);

	case RPC_TYPE_DOUBLE:
		return ((size_t)object->ro_value.rv_d);

	case RPC_TYPE_FD:
		return ((size_t)object->ro_value.rv_fd);

	case RPC_TYPE_DATE:
		return ((size_t)rpc_date_get_value(object));

	case RPC_TYPE_STRING:
		return (g_string_hash(object->ro_value.rv_str));

	case RPC_TYPE_BINARY:
		return (rpc_data_hash((uint8_t *)rpc_data_get_bytes_ptr(object),
		    rpc_data_get_length(object)));

#if defined(__linux__)
	case RPC_TYPE_SHMEM:
		return ((size_t)object->ro_value.rv_shmem->rsb_fd);
#endif

	case RPC_TYPE_DICTIONARY:
		rpc_dictionary_apply(object, ^(const char *k, rpc_object_t v) {
		    	hash ^= rpc_data_hash((const uint8_t *)k, strlen(k));
		    	hash ^= rpc_hash(v);
		    	return ((bool)true);
		});
		return (hash);

	case RPC_TYPE_ARRAY:
		rpc_array_apply(object, ^(size_t idx __unused, rpc_object_t v) {
		    	hash ^= rpc_hash(v);
		    	return ((bool)true);
		});
		return (hash);
	}

	return (0);
}

inline char *
rpc_copy_description(rpc_object_t object)
{
	GString *description;

	description = g_string_new(NULL);
	rpc_create_description(description, object, 0, false);
	g_string_truncate(description, description->len - 1);

	return g_string_free(description, false);
}

rpc_object_t
rpc_object_pack(const char *fmt, ...)
{
	GQueue *stack = g_queue_new();
	GQueue *keys = g_queue_new();
	rpc_object_t current = NULL;
	rpc_object_t container = NULL;
	va_list ap;
	const char *key;
	char ch;

	va_start(ap, fmt);

	while ((ch = *fmt++) != '\0') {
		if (rpc_get_type(container) == RPC_TYPE_DICTIONARY && ch != '}') {
			key = va_arg(ap, const char *);
			g_queue_push_tail(keys, (gpointer)key);
		}

		switch (ch) {
		case 'v':
			current = va_arg(ap, rpc_object_t);
			break;

		case 'n':
			current = rpc_null_create();
			break;

		case 'b':
			current = rpc_bool_create(va_arg(ap, int));
			break;

		case 'B':
			current = rpc_data_create(va_arg(ap, const void *),
			    va_arg(ap, size_t), va_arg(ap, int));
			break;

		case 'f':
			current = rpc_fd_create(va_arg(ap, int));
			break;

		case 'i':
			current = rpc_int64_create(va_arg(ap, int64_t));
			break;

		case 'u':
			current = rpc_uint64_create(va_arg(ap, uint64_t));
			break;

		case 'd':
			current = rpc_double_create(va_arg(ap, double));
			break;

		case 's':
			current = rpc_string_create(va_arg(ap, const char *));
			break;

#if defined(__linux__)
		case 'h':
			current = rpc_shmem_create(va_arg(ap, rpc_shmem_block_t));
			break;
#endif

		case '{':
			container = rpc_dictionary_create();
			g_queue_push_tail(stack, container);
			continue;

		case '[':
			container = rpc_array_create();
			g_queue_push_tail(stack, container);
			continue;

		case '}':
		case ']':
			current = g_queue_pop_tail(stack);
			container = g_queue_peek_tail(stack);
			break;

		default:
			rpc_release(current);
			errno = EINVAL;
			return (NULL);
		}

		if (container != NULL) {
			if (rpc_get_type(container) == RPC_TYPE_DICTIONARY) {
				key = g_queue_pop_tail(keys);
				rpc_dictionary_steal_value(container, key,
				    current);
			}

			if (rpc_get_type(container) == RPC_TYPE_ARRAY) {
				rpc_array_append_stolen_value(container,
				    current);
			}

			continue;
		}

		return (current);
	}

	return (NULL);
}

int
rpc_object_unpack(rpc_object_t obj, const char *fmt, ...)
{
	rpc_object_t array = NULL;
	rpc_object_t current;
	va_list ap;
	char ch;
	size_t idx = 0;

	current = obj;
	va_start(ap, fmt);

	while ((ch = *fmt++) != '\0') {
		if (array)
			current = rpc_array_get_value(array, idx++);

		switch (ch) {
		case '*':
			break;

		case 'v':
			*va_arg(ap, rpc_object_t *) = current;
			break;

		case 'b':
			*va_arg(ap, bool *) = rpc_bool_get_value(current);
			break;

		case 'i':
			*va_arg(ap, int64_t *) = rpc_int64_get_value(current);
			break;

		case 'u':
			*va_arg(ap, uint64_t *) = rpc_uint64_get_value(current);
			break;

		case 'd':
			*va_arg(ap, double *) = rpc_double_get_value(current);
			break;

		case 'f':
			*va_arg(ap, int *) = rpc_fd_get_value(current);
			break;

		case 's':
			*va_arg(ap, const char **) = rpc_string_get_string_ptr(
			    current);
			break;

#if defined(__linux__)
		case 'h':
			*va_arg(ap, rpc_shmem_block_t *) = rpc_shmem_get_block(
			    current);
			break;
#endif

		case '[':
			array = current;
			idx = 0;
			break;

		case ']':
			array = NULL;
			break;

		default:
			return (-1);
		}
	}

	return (0);
}


inline rpc_object_t
rpc_null_create(void)
{
	union rpc_value val = { 0 };

	val.rv_b = false;
	return (rpc_prim_create(RPC_TYPE_NULL, val));
}

inline rpc_object_t
rpc_bool_create(bool value)
{
	union rpc_value val;

	val.rv_b = value;
	return (rpc_prim_create(RPC_TYPE_BOOL, val));
}

inline bool
rpc_bool_get_value(rpc_object_t xbool)
{

	if (xbool->ro_type != RPC_TYPE_BOOL)
		return (false);

	return (xbool->ro_value.rv_b);
}

inline rpc_object_t
rpc_int64_create(int64_t value)
{
	union rpc_value val;

	val.rv_i = value;
	return (rpc_prim_create(RPC_TYPE_INT64, val));
}

inline int64_t
rpc_int64_get_value(rpc_object_t xint)
{

	if (xint->ro_type != RPC_TYPE_INT64)
		return (-1);

	return (xint->ro_value.rv_i);
}

inline rpc_object_t
rpc_uint64_create(uint64_t value)
{
	union rpc_value val;

	val.rv_ui = value;
	return (rpc_prim_create(RPC_TYPE_UINT64, val));
}

inline uint64_t
rpc_uint64_get_value(rpc_object_t xuint)
{

	if (xuint->ro_type != RPC_TYPE_UINT64)
		return (0);

	return (xuint->ro_value.rv_ui);
}

inline rpc_object_t
rpc_double_create(double value)
{
	union rpc_value val;

	val.rv_d = value;
	return (rpc_prim_create(RPC_TYPE_DOUBLE, val));
}

inline double
rpc_double_get_value(rpc_object_t xdouble)
{

	if (xdouble->ro_type != RPC_TYPE_DOUBLE)
		return (0);

	return (xdouble->ro_value.rv_d);
}

inline rpc_object_t
rpc_date_create(int64_t interval)
{
	union rpc_value val;

	val.rv_datetime = g_date_time_new_from_unix_utc(interval);
	return (rpc_prim_create(RPC_TYPE_DATE, val));
}

inline rpc_object_t
rpc_date_create_from_current(void)
{
	union rpc_value val;

	val.rv_datetime = g_date_time_new_now_utc();
	return (rpc_prim_create(RPC_TYPE_DATE, val));
}

inline int64_t
rpc_date_get_value(rpc_object_t xdate)
{

	if (xdate->ro_type != RPC_TYPE_DATE)
		return (0);

	return (g_date_time_to_unix(xdate->ro_value.rv_datetime));
}

inline rpc_object_t
rpc_data_create(const void *bytes, size_t length, bool copy)
{
	union rpc_value value;

	if (copy) {
		value.rv_bin.ptr = (uintptr_t)malloc(length);
		memcpy((void *)value.rv_bin.ptr, bytes, length);
	} else
		value.rv_bin.ptr = (uintptr_t)bytes;

	value.rv_bin.copy = copy;
	value.rv_bin.length = length;

	return (rpc_prim_create(RPC_TYPE_BINARY, value));
}

inline size_t
rpc_data_get_length(rpc_object_t xdata)
{

	if (xdata->ro_type != RPC_TYPE_BINARY)
		return (0);

	return (xdata->ro_value.rv_bin.length);
}

inline const void *
rpc_data_get_bytes_ptr(rpc_object_t xdata)
{

	if (xdata->ro_type != RPC_TYPE_BINARY)
		return (NULL);

	return ((const void *)xdata->ro_value.rv_bin.ptr);
}

inline size_t
rpc_data_get_bytes(rpc_object_t xdata, void *buffer, size_t off, size_t length)
{
	size_t cpy_size;
	size_t xdata_length = rpc_data_get_length(xdata);

	if (xdata->ro_type != RPC_TYPE_BINARY)
		return (0);

	if (off > xdata_length)
		return (0);

	cpy_size = MIN(length, xdata_length - off);

	memcpy(buffer, rpc_data_get_bytes_ptr(xdata) + off, cpy_size);

	return (cpy_size);
}

inline rpc_object_t
rpc_string_create(const char *string)
{
	union rpc_value val;

	val.rv_str = g_string_new(string);
	return (rpc_prim_create(RPC_TYPE_STRING, val));
}

inline rpc_object_t
rpc_string_create_len(const char *string, size_t length)
{
	union rpc_value val;

	val.rv_str = g_string_new_len(string, length);
	return (rpc_prim_create(RPC_TYPE_STRING, val));
}

inline rpc_object_t
rpc_string_create_with_format(const char *fmt, ...)
{
	va_list ap;
	union rpc_value val;

	va_start(ap, fmt);
	val.rv_str = g_string_new(NULL);
	g_string_vprintf(val.rv_str, fmt, ap);
	va_end(ap);

	return (rpc_prim_create(RPC_TYPE_STRING, val));
}

inline rpc_object_t
rpc_string_create_with_format_and_arguments(const char *fmt, va_list ap)
{
	union rpc_value val;

	val.rv_str = g_string_new(NULL);
	g_string_vprintf(val.rv_str, fmt, ap);
	return (rpc_prim_create(RPC_TYPE_STRING, val));
}

inline size_t
rpc_string_get_length(rpc_object_t xstring)
{

	if (xstring->ro_type != RPC_TYPE_STRING)
		return (0);

	return (xstring->ro_value.rv_str->len);
}

inline const char *
rpc_string_get_string_ptr(rpc_object_t xstring)
{

	if (xstring->ro_type != RPC_TYPE_STRING)
		return (NULL);

	return (xstring->ro_value.rv_str->str);
}

inline rpc_object_t
rpc_fd_create(int fd)
{
	union rpc_value val;

	val.rv_fd = fd;
	return (rpc_prim_create(RPC_TYPE_FD, val));
}

inline int
rpc_fd_get_value(rpc_object_t xfd)
{

	if (xfd->ro_type != RPC_TYPE_FD)
		return (-1);

	return (xfd->ro_value.rv_fd);
}

inline int
rpc_fd_dup(rpc_object_t xfd)
{

	if (xfd->ro_type != RPC_TYPE_FD)
		return (0);

	return (dup(rpc_fd_get_value(xfd)));
}

#if defined(__linux__)
rpc_object_t
rpc_shmem_create(rpc_shmem_block_t block)
{
	union rpc_value val;

	val.rv_shmem = block;
	return (rpc_prim_create(RPC_TYPE_SHMEM, val));
}

rpc_shmem_block_t
rpc_shmem_alloc(size_t size)
{
	rpc_shmem_block_t block;

	if (size == 0)
		return (NULL);

	block = g_malloc(sizeof(*block));
	block->rsb_addr = NULL;
	block->rsb_size = size;
	block->rsb_offset = 0;
	block->rsb_fd = memfd_create("librpc", 0);

	if (ftruncate(block->rsb_fd, (off_t)size) != 0) {
		close(block->rsb_fd);
		g_free(block);
		return (NULL);
	}

	block->rsb_addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
			       block->rsb_fd, 0);

	return (block);
}

void
rpc_shmem_free(rpc_shmem_block_t block)
{

	if (block == NULL)
		return;

	munmap(block->rsb_addr, block->rsb_size);
	close(block->rsb_fd);
	g_free(block);
}

void *
rpc_shmem_map(rpc_shmem_block_t block)
{

	return (mmap(NULL, block->rsb_size, PROT_READ | PROT_WRITE, MAP_SHARED,
		     block->rsb_fd, block->rsb_offset));
}

void *
rpc_shmem_block_get_ptr(rpc_shmem_block_t block)
{

	return (block->rsb_addr);
}

size_t
rpc_shmem_block_get_size(rpc_shmem_block_t block)
{

	return (block->rsb_size);
}

rpc_shmem_block_t
rpc_shmem_get_block(rpc_object_t obj)
{

	if (rpc_get_type(obj) != RPC_TYPE_SHMEM)
		return (NULL);

	return (obj->ro_value.rv_shmem);
}
#endif

inline rpc_object_t
rpc_array_create(void)
{
	union rpc_value val;

	val.rv_list = g_ptr_array_new_with_free_func(
	    (GDestroyNotify)rpc_release_impl);
	return (rpc_prim_create(RPC_TYPE_ARRAY, val));
}

inline rpc_object_t
rpc_array_create_ex(const rpc_object_t *objects, size_t count, bool steal)
{
	rpc_object_t array_object;
	size_t i;
	void (*setter_fn)(rpc_object_t, rpc_object_t);

	setter_fn = steal ? &rpc_array_append_stolen_value :
	    &rpc_array_append_value;

	array_object = rpc_array_create();
	for (i = 0; i < count; i++)
		setter_fn(array_object, objects[i]);

	return array_object;
}

inline void
rpc_array_set_value(rpc_object_t array, size_t index, rpc_object_t value)
{
	if (value == NULL)
		rpc_array_remove_index(array, index);
	else {
		rpc_array_steal_value(array, index, value);
		rpc_retain(value);
	}
}

inline void
rpc_array_steal_value(rpc_object_t array, size_t index, rpc_object_t value)
{
	rpc_object_t *ro;
	int i;

	if (array->ro_type != RPC_TYPE_ARRAY)
		abort();

	for (i = (int)(index - array->ro_value.rv_list->len); i > 0; i--) {
		rpc_array_append_stolen_value(
		    array,
		    rpc_null_create()
		);
	}

	if (index == array->ro_value.rv_list->len) {
		rpc_array_append_stolen_value(array, value);
		return;
	}

	ro = (rpc_object_t *)&g_ptr_array_index(array->ro_value.rv_list, index);
	rpc_release_impl(*ro);
	*ro = value;
}

inline void
rpc_array_remove_index(rpc_object_t array, size_t index)
{
	if (array->ro_type != RPC_TYPE_ARRAY)
		abort();

	if (index >= rpc_array_get_count(array))
		return;

	g_ptr_array_remove_index(array->ro_value.rv_list, (guint)index);
}


inline void
rpc_array_append_value(rpc_object_t array, rpc_object_t value)
{

	rpc_array_append_stolen_value(array, value);
	rpc_retain(value);
}

inline void
rpc_array_append_stolen_value(rpc_object_t array, rpc_object_t value)
{

	if (array->ro_type != RPC_TYPE_ARRAY)
		abort();

	g_ptr_array_add(array->ro_value.rv_list, value);
}

inline rpc_object_t
rpc_array_get_value(rpc_object_t array, size_t index)
{
	if (array->ro_type != RPC_TYPE_ARRAY)
		return (NULL);

	if (index >= array->ro_value.rv_list->len)
		return (NULL);

	return (g_ptr_array_index(array->ro_value.rv_list, index));
}

inline size_t
rpc_array_get_count(rpc_object_t array)
{
	if (array->ro_type != RPC_TYPE_ARRAY)
		return (0);

	return (array->ro_value.rv_list->len);
}

inline bool
rpc_array_apply(rpc_object_t array, rpc_array_applier_t applier)
{
	bool flag = false;
	size_t i;

	for (i = 0; i < array->ro_value.rv_list->len; i++) {
		if (!applier(i, g_ptr_array_index(array->ro_value.rv_list, i))) {
			flag = true;
			break;
		}
	}

	return (flag);
}

rpc_object_t
rpc_array_slice(rpc_object_t array, size_t index, ssize_t len)
{
	size_t i;
	size_t end;
	rpc_object_t result;

	if (len == -1)
		end = array->ro_value.rv_list->len;
	else
		end = MIN(array->ro_value.rv_list->len, index + len)

	result = rpc_array_create();

	for (i = index; i < end; i++)
		rpc_array_append_value(result, rpc_array_get_value(array, i));

	return (result;)
}

inline void
rpc_array_set_bool(rpc_object_t array, size_t index, bool value)
{

	rpc_array_steal_value(array, index, rpc_bool_create(value));
}

inline void
rpc_array_set_int64(rpc_object_t array, size_t index, int64_t value)
{

	rpc_array_steal_value(array, index, rpc_int64_create(value));
}

inline void
rpc_array_set_uint64(rpc_object_t array, size_t index, uint64_t value)
{

	rpc_array_steal_value(array, index, rpc_uint64_create(value));
}

inline void
rpc_array_set_double(rpc_object_t array, size_t index, double value)
{

	rpc_array_steal_value(array, index, rpc_double_create(value));
}

inline void
rpc_array_set_date(rpc_object_t array, size_t index, int64_t value)
{

	rpc_array_steal_value(array, index, rpc_date_create(value));
}

inline void
rpc_array_set_data(rpc_object_t array, size_t index, const void *bytes,
    size_t length)
{

	rpc_array_steal_value(array, index, rpc_data_create(bytes, length,
	    false));
}

inline void
rpc_array_set_string(rpc_object_t array, size_t index,
    const char *value)
{

	rpc_array_steal_value(array, index, rpc_string_create(value));
}

inline void
rpc_array_set_fd(rpc_object_t array, size_t index, int value)
{

	rpc_array_steal_value(array, index, rpc_fd_create(value));
}

inline bool
rpc_array_get_bool(rpc_object_t array, size_t index)
{

	if (index >= rpc_array_get_count(array))
		return (false);

	return (rpc_bool_get_value(rpc_array_get_value(array, index)));
}

inline int64_t
rpc_array_get_int64(rpc_object_t array, size_t index)
{

	if (index >= rpc_array_get_count(array))
		return (0);

	return (rpc_int64_get_value(rpc_array_get_value(array, index)));
}

inline uint64_t
rpc_array_get_uint64(rpc_object_t array, size_t index)
{
	if (index >= rpc_array_get_count(array))
		return (0);

	return (rpc_uint64_get_value(rpc_array_get_value(array, index)));
}

inline double
rpc_array_get_double(rpc_object_t array, size_t index)
{

	if (index >= rpc_array_get_count(array))
		return (0);

	return (rpc_double_get_value(rpc_array_get_value(array, index)));
}

inline int64_t
rpc_array_get_date(rpc_object_t array, size_t index)
{

	if (index >= rpc_array_get_count(array))
		return (0);

	return (rpc_date_get_value(rpc_array_get_value(array, index)));
}

inline const void *
rpc_array_get_data(rpc_object_t array, size_t index, size_t *length)
{
	rpc_object_t xdata;

	if (index >= rpc_array_get_count(array))
		return (NULL);

	if ((xdata = rpc_array_get_value(array, index)) == 0)
		return (NULL);

	if (length != NULL)
		*length = xdata->ro_value.rv_bin.length;

	return rpc_data_get_bytes_ptr(xdata);
}

inline const char *
rpc_array_get_string(rpc_object_t array, size_t index)
{

	if (index >= rpc_array_get_count(array))
		return (NULL);

	return rpc_string_get_string_ptr(rpc_array_get_value(array, index));
}

inline int
rpc_array_get_fd(rpc_object_t array, size_t index)
{

	if (index >= rpc_array_get_count(array))
		return (0);

	return (rpc_fd_get_value(rpc_array_get_value(array, index)));
}

inline int
rpc_array_dup_fd(rpc_object_t array, size_t index)
{

	if (index >= rpc_array_get_count(array))
		return (0);

	return (rpc_fd_dup(rpc_array_get_value(array, index)));
}

inline rpc_object_t
rpc_dictionary_create(void)
{
	union rpc_value val;

	val.rv_dict = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
	    (GDestroyNotify)rpc_release_impl);

	return (rpc_prim_create(RPC_TYPE_DICTIONARY, val));
}

inline rpc_object_t
rpc_dictionary_create_ex(const char * const *keys, const rpc_object_t *values,
    size_t count, bool steal)
{
	rpc_object_t object;
	size_t i;
	void (*setter_fn)(rpc_object_t, const char *, rpc_object_t);

	setter_fn = steal ? &rpc_dictionary_steal_value :
	    &rpc_dictionary_set_value;

	object = rpc_dictionary_create();

	for (i = 0; i < count; i++)
		setter_fn(object, keys[i], values[i]);

	return (object);
}

inline void
rpc_dictionary_set_value(rpc_object_t dictionary, const char *key,
    rpc_object_t value)
{
	if (value == NULL)
		rpc_dictionary_remove_key(dictionary, key);
	else {
		rpc_dictionary_steal_value(dictionary, key, value);
		rpc_retain(value);
	}
}

inline void
rpc_dictionary_steal_value(rpc_object_t dictionary, const char *key,
    rpc_object_t value)
{

	if (dictionary->ro_type != RPC_TYPE_DICTIONARY)
		abort();

	g_hash_table_insert(dictionary->ro_value.rv_dict,
	    (gpointer)g_strdup(key), value);
}

inline void
rpc_dictionary_remove_key(rpc_object_t dictionary, const char *key)
{
	if (dictionary->ro_type != RPC_TYPE_DICTIONARY)
		abort();

	g_hash_table_remove(dictionary->ro_value.rv_dict, key);
}

inline rpc_object_t
rpc_dictionary_get_value(rpc_object_t dictionary,
    const char *key)
{

	return ((rpc_object_t)g_hash_table_lookup(
	    dictionary->ro_value.rv_dict, key));
}

inline size_t
rpc_dictionary_get_count(rpc_object_t dictionary)
{

	return ((size_t)g_hash_table_size(dictionary->ro_value.rv_dict));
}

inline bool
rpc_dictionary_apply(rpc_object_t dictionary, rpc_dictionary_applier_t applier)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init(&iter, dictionary->ro_value.rv_dict);

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		if (!applier((const char *)key, (rpc_object_t)value))
			break;
	}

	return (true);
}

inline bool
rpc_dictionary_has_key(rpc_object_t dictionary, const char *key)
{

	return (g_hash_table_lookup(dictionary->ro_value.rv_dict, key) != NULL);
}

inline void
rpc_dictionary_set_bool(rpc_object_t dictionary, const char *key, bool value)
{

	rpc_dictionary_steal_value(dictionary, key, rpc_bool_create(value));
}

inline void
rpc_dictionary_set_int64(rpc_object_t dictionary, const char *key,
    int64_t value)
{

	rpc_dictionary_steal_value(dictionary, key, rpc_int64_create(value));
}

inline void
rpc_dictionary_set_uint64(rpc_object_t dictionary, const char *key,
    uint64_t value)
{

	rpc_dictionary_steal_value(dictionary, key, rpc_uint64_create(value));
}

inline void
rpc_dictionary_set_double(rpc_object_t dictionary, const char *key,
    double value)
{

	rpc_dictionary_steal_value(dictionary, key, rpc_double_create(value));
}

inline void
rpc_dictionary_set_date(rpc_object_t dictionary, const char *key,
    int64_t value)
{

	rpc_dictionary_steal_value(dictionary, key, rpc_date_create(value));
}

inline void
rpc_dictionary_set_data(rpc_object_t dictionary, const char *key,
    const void *value, size_t length)
{

	rpc_dictionary_steal_value(dictionary, key, rpc_data_create(value,
	    length, false));
}

inline void
rpc_dictionary_set_string(rpc_object_t dictionary, const char *key,
    const char *value)
{

	rpc_dictionary_steal_value(dictionary, key, rpc_string_create(value));
}

inline void
rpc_dictionary_set_fd(rpc_object_t dictionary, const char *key, int value)
{

	rpc_dictionary_steal_value(dictionary, key, rpc_fd_create(value));
}

inline bool
rpc_dictionary_get_bool(rpc_object_t dictionary, const char *key)
{
	rpc_object_t xbool;

	xbool = rpc_dictionary_get_value(dictionary, key);
	return ((xbool != NULL) ? rpc_bool_get_value(xbool) : false);
}

inline int64_t
rpc_dictionary_get_int64(rpc_object_t dictionary, const char *key)
{
	rpc_object_t xint;

	xint = rpc_dictionary_get_value(dictionary, key);
	return ((xint != NULL) ? rpc_int64_get_value(xint) : 0);
}

inline uint64_t
rpc_dictionary_get_uint64(rpc_object_t dictionary, const char *key)
{
	rpc_object_t xuint;

	xuint = rpc_dictionary_get_value(dictionary, key);
	return ((xuint != NULL) ? rpc_uint64_get_value(xuint) : 0);
}

inline double
rpc_dictionary_get_double(rpc_object_t dictionary, const char *key)
{
	rpc_object_t xdouble;

	xdouble = rpc_dictionary_get_value(dictionary, key);
	return ((xdouble != NULL) ? rpc_double_get_value(xdouble) : 0);
}

inline int64_t
rpc_dictionary_get_date(rpc_object_t dictionary, const char *key)
{
	rpc_object_t xdate;

	xdate = rpc_dictionary_get_value(dictionary, key);
	return ((xdate != NULL) ? rpc_date_get_value(xdate) : false);
}

inline const void *
rpc_dictionary_get_data(rpc_object_t dictionary, const char *key,
    size_t *length)
{
	rpc_object_t xdata;

	if ((xdata = rpc_dictionary_get_value(dictionary, key)) == NULL)
		return (NULL);

	if (length != NULL)
		*length = xdata->ro_value.rv_bin.length;

	return rpc_data_get_bytes_ptr(xdata);
}

inline const char *
rpc_dictionary_get_string(rpc_object_t dictionary, const char *key)
{
	rpc_object_t xstring;

	xstring = rpc_dictionary_get_value(dictionary, key);
	return ((xstring != NULL) ? rpc_string_get_string_ptr(xstring) : NULL);
}

inline int
rpc_dictionary_get_fd(rpc_object_t dictionary, const char *key)
{
	rpc_object_t xfd;

	xfd = rpc_dictionary_get_value(dictionary, key);
	return ((xfd != NULL) ? rpc_fd_get_value(xfd) : 0);
}

inline int
rpc_dictionary_dup_fd(rpc_object_t dictionary, const char *key)
{
	rpc_object_t xfd;

	xfd = rpc_dictionary_get_value(dictionary, key);
	return (xfd != NULL ? rpc_fd_dup(xfd) : 0);
}
