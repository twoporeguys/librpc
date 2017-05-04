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
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <rpc/object.h>
#include "internal.h"

static rpc_object_t
rpc_prim_create(rpc_type_t type, union rpc_value val, size_t size)
{
	struct rpc_object *ro;

	ro = (rpc_object_t)malloc(sizeof(*ro));
	if (ro == NULL)
		abort();

	ro->ro_type = type;
	ro->ro_size = size;
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

inline rpc_object_t
rpc_retain(rpc_object_t object)
{
	g_atomic_int_inc(&object->ro_refcnt);
	return (object);
}

inline void
rpc_release(rpc_object_t object)
{
	assert(object->ro_refcnt > 0);
	if (g_atomic_int_dec_and_test(&object->ro_refcnt)) {
		free(object);
	}
}

inline rpc_object_t
rpc_copy(rpc_object_t object)
{
	rpc_object_t tmp;
        union rpc_value tmp_value;

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

	case RPC_TYPE_STRING:
		return (rpc_string_create(strdup(
		    rpc_string_get_string_ptr(object))));

	case RPC_TYPE_BINARY:
                tmp_value.rv_ptr = (uintptr_t)rpc_data_get_bytes_ptr(object);
		return (rpc_prim_create(
                    object->ro_type,
                    tmp_value,
		    rpc_data_get_length(object)));

	case RPC_TYPE_DICTIONARY:
		tmp = rpc_dictionary_create(NULL, NULL, 0);
		rpc_dictionary_apply(object, ^(const char *k, rpc_object_t v) {
		    rpc_dictionary_set_value(tmp, k, rpc_copy(v));
		    return ((bool)true);
		});
		return (tmp);

	case RPC_TYPE_ARRAY:
		tmp = rpc_array_create(NULL, 0);
		rpc_array_apply(object, ^(size_t idx, rpc_object_t v) {
		    rpc_array_set_value(tmp, idx, rpc_copy(v));
		    return ((bool)true);
		});
		return (tmp);
	}

	return (0);
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
	case RPC_TYPE_INT64:
	case RPC_TYPE_UINT64:
	case RPC_TYPE_DOUBLE:
	case RPC_TYPE_FD:
		return ((size_t)object->ro_value.rv_ui);

	case RPC_TYPE_DATE:
		return ((size_t)rpc_date_get_value(object));

	case RPC_TYPE_STRING:
		return (g_string_hash(object->ro_value.rv_str));

	case RPC_TYPE_BINARY:
		return (rpc_data_hash(
                    (uint8_t *)rpc_data_get_bytes_ptr(object),
		    rpc_data_get_length(object)));

	case RPC_TYPE_DICTIONARY:
		rpc_dictionary_apply(object, ^(const char *k, rpc_object_t v) {
		    hash ^= rpc_data_hash((const uint8_t *)k, strlen(k));
		    hash ^= rpc_hash(v);
		    return ((bool)true);
		});
		return (hash);

	case RPC_TYPE_ARRAY:
		rpc_array_apply(object, ^(size_t idx, rpc_object_t v) {
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

}

inline rpc_object_t
rpc_null_create(void)
{
	union rpc_value val;

	return (rpc_prim_create(RPC_TYPE_NULL, val, 0));
}

inline rpc_object_t
rpc_bool_create(bool value)
{
	union rpc_value val;

	val.rv_b = value;
	return (rpc_prim_create(RPC_TYPE_BOOL, val, 1));
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
	return (rpc_prim_create(RPC_TYPE_INT64, val, 1));
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
	return (rpc_prim_create(RPC_TYPE_UINT64, val, 1));
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
	return (rpc_prim_create(RPC_TYPE_DOUBLE, val, 1));
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
	return (rpc_prim_create(RPC_TYPE_DATE, val, 1));
}

inline rpc_object_t rpc_date_create_from_current(void)
{
        union rpc_value val;

        val.rv_datetime = g_date_time_new_now_utc();
        return (rpc_prim_create(RPC_TYPE_DATE, val, 1));
}

inline int64_t
rpc_date_get_value(rpc_object_t xdate)
{
	if (xdate->ro_type != RPC_TYPE_DATE)
		return (0);

	return (g_date_time_to_unix(xdate->ro_value.rv_datetime));
}

inline rpc_object_t
rpc_data_create(const void *bytes, size_t length)
{

}

inline size_t
rpc_data_get_length(rpc_object_t xdata)
{
	if (xdata->ro_type != RPC_TYPE_BINARY)
		return (0);

	return (xdata->ro_size);
}

inline const void *
rpc_data_get_bytes_ptr(rpc_object_t xdata)
{
	if (xdata->ro_type != RPC_TYPE_BINARY)
		return (NULL);

	return ((const void *)xdata->ro_value.rv_ptr);
}

inline size_t
rpc_data_get_bytes(rpc_object_t xdata, void *buffer, size_t off,
    size_t length)
{

}

inline rpc_object_t
rpc_string_create(const char *string)
{
	union rpc_value val;
	const char *str;

	str = g_strdup(string);
	val.rv_str = g_string_new(str);
	return (rpc_prim_create(RPC_TYPE_STRING, val, strlen(str)));
}

inline rpc_object_t
rpc_string_create_with_format(const char *fmt, ...)
{
	va_list ap;
	union rpc_value val;

	va_start(ap, fmt);
	g_vasprintf(&val.rv_str, fmt, ap);
	va_end(ap);

	return (rpc_prim_create(RPC_TYPE_STRING, val, 1));
}

inline rpc_object_t
rpc_string_create_with_format_and_arguments(const char *fmt, va_list ap)
{
	union rpc_value val;

	g_vasprintf(&val.rv_str, fmt, ap);
	return (rpc_prim_create(RPC_TYPE_STRING, val, 1));
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
		return (0);

	return (xstring->ro_value.rv_str->str);
}

inline rpc_object_t
rpc_fd_create(int fd)
{
	union rpc_value val;

	val.rv_fd = fd;
	return (rpc_prim_create(RPC_TYPE_FD, val, 1));
}

inline int
rpc_fd_dup(rpc_object_t xfd)
{

}

inline rpc_object_t
rpc_array_create(const rpc_object_t *objects, size_t count)
{

	union rpc_value val;

	val.rv_list = g_array_new(true, true, sizeof(rpc_object_t));
	return (rpc_prim_create(RPC_TYPE_ARRAY, val, 1));
}

inline void
rpc_array_set_value(rpc_object_t array, size_t index, rpc_object_t value)
{
	rpc_object_t *ro;

	rpc_retain(value);
	ro = &g_array_index(array->ro_value.rv_list, rpc_object_t, index);
	rpc_release(*ro);
	*ro = value;
}

inline void
rpc_array_append_value(rpc_object_t array, rpc_object_t value)
{

	rpc_retain(value);
	g_array_append_val(array->ro_value.rv_list, value);
}

inline rpc_object_t
rpc_array_get_value(rpc_object_t array, size_t index)
{

	return (g_array_index(array->ro_value.rv_list, rpc_object_t, index));
}

inline size_t rpc_array_get_count(rpc_object_t array)
{
	if (array->ro_type != RPC_TYPE_ARRAY)
		return (0);

	return (array->ro_size);
}

inline bool
rpc_array_apply(rpc_object_t array, rpc_array_applier_t applier)
{
	bool flag = false;
	size_t i = 0;

	for (i = 0; i < array->ro_value.rv_list->len; i++) {
		if (!applier(i, g_array_index(array->ro_value.rv_list,
		    rpc_object_t, i))) {
			flag = true;
			break;
		}
	}

	return (flag);
}

inline void
rpc_array_set_bool(rpc_object_t array, size_t index, bool value)
{

	rpc_array_set_value(array, index, rpc_bool_create(value));
}

inline void
rpc_array_set_int64(rpc_object_t array, size_t index, int64_t value)
{

	rpc_array_set_value(array, index, rpc_int64_create(value));
}

inline void
rpc_array_set_uint64(rpc_object_t array, size_t index, uint64_t value)
{

	rpc_array_set_value(array, index, rpc_uint64_create(value));
}

inline void
rpc_array_set_double(rpc_object_t array, size_t index, double value)
{

	rpc_array_set_value(array, index, rpc_double_create(value));
}

inline void
rpc_array_set_date(rpc_object_t array, size_t index, int64_t value)
{

	rpc_array_set_value(array, index, rpc_date_create(value));
}

inline void
rpc_array_set_data(rpc_object_t array, size_t index, const void *bytes,
    size_t length)
{

	rpc_array_set_value(array, index, rpc_data_create(bytes, length));
}

inline void rpc_array_set_string(rpc_object_t array, size_t index,
    const char *value)
{

	rpc_array_set_value(array, index, rpc_string_create(value));
}

inline void
rpc_array_set_fd(rpc_object_t array, size_t index, int value)
{

	rpc_array_set_value(array, index, rpc_fd_create(value));
}

inline bool
rpc_array_get_bool(rpc_object_t array, size_t index)
{

	return rpc_bool_get_value(rpc_array_get_value(array, index));
}

inline int64_t
rpc_array_get_int64(rpc_object_t array, size_t index)
{

	return rpc_int64_get_value(rpc_array_get_value(array, index));
}

inline uint64_t
rpc_array_get_uint64(rpc_object_t array, size_t index)
{

	return rpc_uint64_get_value(rpc_array_get_value(array, index));
}

inline double
rpc_array_get_double(rpc_object_t array, size_t index)
{

	return rpc_double_get_value(rpc_array_get_value(array, index));
}

inline int64_t
rpc_array_get_date(rpc_object_t array, size_t index)
{

	return rpc_date_get_value(rpc_array_get_value(array, index));
}

inline const void *
rpc_array_get_data(rpc_object_t array, size_t index,
    size_t *length)
{

}

inline const char *
rpc_array_get_string(rpc_object_t array, size_t index)
{

	return rpc_string_get_string_ptr(rpc_array_get_value(array, index));
}

inline int rpc_array_dup_fd(rpc_object_t array, size_t index)
{

}

inline rpc_object_t
rpc_dictionary_create(const char * const *keys, const rpc_object_t *values,
    size_t count)
{
	union rpc_value val;

	val.rv_dict = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
	    (GDestroyNotify)rpc_release);

	return (rpc_prim_create(RPC_TYPE_DICTIONARY, val, 0));
}

inline void
rpc_dictionary_set_value(rpc_object_t dictionary, const char *key,
    rpc_object_t value)
{
	if (dictionary->ro_type != RPC_TYPE_DICTIONARY)
		return;

	rpc_retain(value);
	g_hash_table_insert(dictionary->ro_value.rv_dict,
	    rpc_string_create(key), value);
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

inline void
rpc_dictionary_set_bool(rpc_object_t dictionary, const char *key, bool value)
{

	rpc_dictionary_set_value(dictionary, key, rpc_bool_create(value));
}

inline void
rpc_dictionary_set_int64(rpc_object_t dictionary, const char *key,
    int64_t value)
{

	rpc_dictionary_set_value(dictionary, key, rpc_int64_create(value));
}

inline void
rpc_dictionary_set_uint64(rpc_object_t dictionary, const char *key,
    uint64_t value)
{

	rpc_dictionary_set_value(dictionary, key, rpc_uint64_create(value));
}

inline void
rpc_dictionary_set_double(rpc_object_t dictionary, const char *key,
    double value)
{

	rpc_dictionary_set_value(dictionary, key, rpc_double_create(value));
}

inline void
rpc_dictionary_set_date(rpc_object_t dictionary, const char *key,
    int64_t value)
{

	rpc_dictionary_set_value(dictionary, key, rpc_int64_create(value));
}

inline void
rpc_dictionary_set_data(rpc_object_t dictionary, const char *key,
    const void *value, size_t length)
{

	rpc_dictionary_set_value(dictionary, key, rpc_data_create(value, length));
}

inline void
rpc_dictionary_set_string(rpc_object_t dictionary, const char *key,
    const char *value)
{

	rpc_dictionary_set_value(dictionary, key, rpc_string_create(value));
}

inline void
rpc_dictionary_set_fd(rpc_object_t dictionary, const char *key, int value)
{

	rpc_dictionary_set_value(dictionary, key, rpc_fd_create(value));
}

inline bool
rpc_dictionary_get_bool(rpc_object_t dictionary, const char *key)
{

	return rpc_bool_get_value(rpc_dictionary_get_value(dictionary, key));
}

inline int64_t
rpc_dictionary_get_int64(rpc_object_t dictionary, const char *key)
{

	return rpc_int64_get_value(rpc_dictionary_get_value(dictionary, key));
}

inline uint64_t
rpc_dictionary_get_uint64(rpc_object_t dictionary, const char *key)
{

	return rpc_uint64_get_value(rpc_dictionary_get_value(dictionary, key));
}

inline double
rpc_dictionary_get_double(rpc_object_t dictionary, const char *key)
{

	return rpc_double_get_value(rpc_dictionary_get_value(dictionary, key));
}

inline int64_t
rpc_dictionary_get_date(rpc_object_t dictionary, const char *key)
{

	return rpc_date_get_value(rpc_dictionary_get_value(dictionary, key));
}

inline const void *
rpc_dictionary_get_data(rpc_object_t dictionary, const char *key,
    size_t *length)
{

}

inline const char *
rpc_dictionary_get_string(rpc_object_t dictionary, const char *key)
{

	return rpc_string_get_string_ptr(rpc_dictionary_get_value(dictionary,
	    key));
}

inline int
rpc_dictionary_dup_fd(rpc_object_t dictionary, const char *key)
{

}
