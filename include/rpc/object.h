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

#ifndef LIBRPC_OBJECT_H
#define LIBRPC_OBJECT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rpc_object;

typedef enum {
	RPC_TYPE_NULL,
	RPC_TYPE_BOOL,
	RPC_TYPE_UINT64,
	RPC_TYPE_INT64,
	RPC_TYPE_DOUBLE,
	RPC_TYPE_DATE,
	RPC_TYPE_STRING,
	RPC_TYPE_BINARY,
	RPC_TYPE_FD,
	RPC_TYPE_DICTIONARY,
	RPC_TYPE_ARRAY
} rpc_type_t;

typedef struct rpc_object *rpc_object_t;
typedef bool (^rpc_array_applier_t)(size_t index, rpc_object_t value);
typedef bool (^rpc_dictionary_applier_t)(const char *key, rpc_object_t value);
typedef void (^rpc_callback_t)(rpc_object_t object);

rpc_object_t rpc_retain(rpc_object_t object);
int rpc_release_impl(rpc_object_t object);
rpc_object_t rpc_copy(rpc_object_t object);
bool rpc_equal(rpc_object_t o1, rpc_object_t o2);
size_t rpc_hash(rpc_object_t object);
char *rpc_copy_description(rpc_object_t object);
rpc_type_t rpc_get_type(rpc_object_t object);

#define	rpc_release(_object)				\
	if (rpc_release_impl(_object) == 0) {		\
		_object = NULL;				\
	}

rpc_object_t rpc_null_create(void);
rpc_object_t rpc_bool_create(bool value);
bool rpc_bool_get_value(rpc_object_t xbool);
rpc_object_t rpc_int64_create(int64_t value);
int64_t rpc_int64_get_value(rpc_object_t xint);
rpc_object_t rpc_uint64_create(uint64_t value);
uint64_t rpc_uint64_get_value(rpc_object_t xuint);
rpc_object_t rpc_double_create(double value);
double rpc_double_get_value(rpc_object_t xdouble);
rpc_object_t rpc_date_create(int64_t interval);
rpc_object_t rpc_date_create_from_current(void);
int64_t rpc_date_get_value(rpc_object_t xdate);
rpc_object_t rpc_data_create(const void *bytes, size_t length, bool copy);
size_t rpc_data_get_length(rpc_object_t xdata);
const void *rpc_data_get_bytes_ptr(rpc_object_t xdata);
size_t rpc_data_get_bytes(rpc_object_t xdata, void *buffer, size_t off,
    size_t length);
rpc_object_t rpc_string_create(const char *string);
rpc_object_t rpc_string_create_with_format(const char *fmt, ...);
rpc_object_t rpc_string_create_with_format_and_arguments(const char *fmt,
    va_list ap);
size_t rpc_string_get_length(rpc_object_t xstring);
const char *rpc_string_get_string_ptr(rpc_object_t xstring);
rpc_object_t rpc_fd_create(int fd);
rpc_object_t rpc_fd_dup(rpc_object_t xfd);
int rpc_fd_get_value(rpc_object_t xfd);

rpc_object_t rpc_array_create(const rpc_object_t *objects, size_t count);
void rpc_array_set_value(rpc_object_t array, size_t index, rpc_object_t value);
void rpc_array_append_value(rpc_object_t array, rpc_object_t value);
rpc_object_t rpc_array_get_value(rpc_object_t array, size_t index);
size_t rpc_array_get_count(rpc_object_t array);
bool rpc_array_apply(rpc_object_t array, rpc_array_applier_t applier);
void rpc_array_set_bool(rpc_object_t array, size_t index, bool value);
void rpc_array_set_int64(rpc_object_t array, size_t index, int64_t value);
void rpc_array_set_uint64(rpc_object_t array, size_t index, uint64_t value);
void rpc_array_set_double(rpc_object_t array, size_t index, double value);
void rpc_array_set_date(rpc_object_t array, size_t index, int64_t value);
void rpc_array_set_data(rpc_object_t array, size_t index, const void *bytes,
    size_t length);
void rpc_array_set_string(rpc_object_t array, size_t index, const char *value);
void rpc_array_set_fd(rpc_object_t array, size_t index, int value);
bool rpc_array_get_bool(rpc_object_t array, size_t index);
int64_t rpc_array_get_int64(rpc_object_t array, size_t index);
uint64_t rpc_array_get_uint64(rpc_object_t array, size_t index);
double rpc_array_get_double(rpc_object_t array, size_t index);
int64_t rpc_array_get_date(rpc_object_t array, size_t index);
const void *rpc_array_get_data(rpc_object_t array, size_t index,
    size_t *length);
const char *rpc_array_get_string(rpc_object_t array, size_t index);
int rpc_array_dup_fd(rpc_object_t array, size_t index);

rpc_object_t rpc_dictionary_create(const char *const *keys,
    const rpc_object_t *values, size_t count);
rpc_object_t rpc_dictionary_create_reply(rpc_object_t original);
void rpc_dictionary_set_value(rpc_object_t dictionary, const char *key,
    rpc_object_t value);
rpc_object_t rpc_dictionary_get_value(rpc_object_t dictionary,
    const char *key);
size_t rpc_dictionary_get_count(rpc_object_t dictionary);
bool rpc_dictionary_apply(rpc_object_t dictionary,
    rpc_dictionary_applier_t applier);
void rpc_dictionary_set_bool(rpc_object_t dictionary, const char *key,
    bool value);
void rpc_dictionary_set_int64(rpc_object_t dictionary, const char *key,
    int64_t value);
void rpc_dictionary_set_uint64(rpc_object_t dictionary, const char *key,
    uint64_t value);
void rpc_dictionary_set_double(rpc_object_t dictionary, const char *key,
    double value);
void rpc_dictionary_set_date(rpc_object_t dictionary, const char *key,
    int64_t value);
void rpc_dictionary_set_data(rpc_object_t dictionary, const char *key,
    const void *value, size_t length);
void rpc_dictionary_set_string(rpc_object_t dictionary, const char *key,
    const char *value);
void rpc_dictionary_set_fd(rpc_object_t dictionary, const char *key,
    int value);
bool rpc_dictionary_get_bool(rpc_object_t dictionary, const char *key);
int64_t rpc_dictionary_get_int64(rpc_object_t dictionary, const char *key);
uint64_t rpc_dictionary_get_uint64(rpc_object_t dictionary, const char *key);
double rpc_dictionary_get_double(rpc_object_t dictionary, const char *key);
int64_t rpc_dictionary_get_date(rpc_object_t dictionary, const char *key);
const void *rpc_dictionary_get_data(rpc_object_t dictionary, const char *key,
    size_t *length);
const char *rpc_dictionary_get_string(rpc_object_t dictionary,
    const char *key);
int rpc_dictionary_dup_fd(rpc_object_t dictionary, const char *key);

#ifdef __cplusplus
}
#endif

#endif //LIBRPC_OBJECT_H
