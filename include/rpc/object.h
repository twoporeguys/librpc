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

/**
 * @file object.h
 *
 * Object model (boxed types) API.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct rpc_object;
#if defined(__linux__)
struct rpc_shmem_block;
#endif

struct rpc_error
{
	int 		code;
	char * 		message;
};

/**
 * Enumerates the possible types of an rpc_object_t.
 */
typedef enum {
	RPC_TYPE_NULL,			/**< null type */
	RPC_TYPE_BOOL,			/**< boolean type */
	RPC_TYPE_UINT64,		/**< unsigned 64-bit integer type */
	RPC_TYPE_INT64,			/**< signed 64-bit integer type */
	RPC_TYPE_DOUBLE,		/**< double precision floating-point type */
	RPC_TYPE_DATE,			/**< date type (represented as 64-bit timestamp */
	RPC_TYPE_STRING,		/**< string type */
	RPC_TYPE_BINARY,		/**< binary data type */
	RPC_TYPE_FD,			/**< file descriptor type */
	RPC_TYPE_DICTIONARY,	/**< dictionary type */
#if defined(__linux__)
    	RPC_TYPE_SHMEM,		/**< shared memory type */
#endif
	RPC_TYPE_ARRAY			/**< array type */
} rpc_type_t;

typedef struct rpc_object *rpc_object_t;
#if defined(__linux__)
typedef struct rpc_shmem_block *rpc_shmem_block_t;
#endif
typedef struct rpc_error *rpc_error_t;
typedef bool (^rpc_array_applier_t)(size_t index, rpc_object_t value);
typedef bool (^rpc_dictionary_applier_t)(const char *key, rpc_object_t value);

/**
 * Converts function pointer to an rpc_array_applier_t block type.
 */
#define	RPC_ARRAY_APPLIER(_fn, _arg)					\
	^(size_t _index, rpc_object_t _value) {				\
                return ((bool)_fn(_arg, _index, _value));		\
        }

/**
 * Converts function pointer to an rpc_dictionary_applier_t block type.
 */
#define	RPC_DICTIONARY_APPLIER(_fn, _arg)				\
	^(const char *_key, rpc_object_t _value) {			\
                return ((bool)_fn(_arg, _key, _value));			\
        }

/**
 * Increases reference count of an object.
 *
 * For convenience, the function returns reference to an object passed
 * as the first argument.
 *
 * @param object Object to increase reference count of.
 * @return Same object
 */
rpc_object_t rpc_retain(rpc_object_t object);

/**
 *
 * @param object
 * @return
 */
int rpc_release_impl(rpc_object_t object);

/**
 *
 * @param object
 * @return Copied object.
 */
rpc_object_t rpc_copy(rpc_object_t object);
bool rpc_equal(rpc_object_t o1, rpc_object_t o2);
size_t rpc_hash(rpc_object_t object);
char *rpc_copy_description(rpc_object_t object);
rpc_type_t rpc_get_type(rpc_object_t object);

/**
 * Decreases reference count of an object and sets it to NULL if needed.
 *
 */
#define	rpc_release(_object)						\
	do {								\
		if (rpc_release_impl(_object) == 0)			\
			_object = NULL;					\
	} while(0)

rpc_error_t rpc_get_last_error(void);

rpc_object_t rpc_object_from_json(const void *frame, size_t size);
int rpc_object_to_json(rpc_object_t object, void **frame, size_t *size);

rpc_object_t rpc_object_pack(const char *fmt, ...);
int rpc_object_unpack(rpc_object_t, const char *fmt, ...);

/**
 * Creates an object holding null value
 *
 * @return newly created object
 */
rpc_object_t rpc_null_create(void);

/**
 * Creates an rpc_object_t holding boolean value
 *
 * @param value Value of the object (true or false)
 * @return Newly created object
 */
rpc_object_t rpc_bool_create(bool value);

/**
 * Returns a boolean value of an object.
 *
 * If rpc_object_t passed as the first argument if not of RPC_TYPE_BOOLEAN
 * type, the function returns false.
 *
 * @param xbool Object to read the value from
 * @return Boolean value of the object (true or false)
 */
bool rpc_bool_get_value(rpc_object_t xbool);

/**
 * Creates an object holding a signed 64-bit integer value
 *
 * @param value Value of the object (signed 64-bit integer)
 * @return Newly created object
 */
rpc_object_t rpc_int64_create(int64_t value);

/**
 * Returns an integer value of an object.
 *
 * If rpc_object_t passed as the first argument if not of RPC_TYPE_INT64
 * type, the function returns -1.
 *
 * @param xint Object to read the value from
 * @return Integer value of the object
 */
int64_t rpc_int64_get_value(rpc_object_t xint);

/**
 * Creates an RPC object holding an unsigned 64-bit integer value
 *
 * @param value Value of the object (unsigned 64-bit integer)
 * @return Newly created object
 */
rpc_object_t rpc_uint64_create(uint64_t value);

/**
 * Returns an integer value of an object.
 *
 * If rpc_object_t passed as the first argument if not of RPC_TYPE_UINT64
 * type, the function returns 0.
 *
 * @param xuint Object to read the value from
 * @return Integer value of the object
 */
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
rpc_object_t rpc_string_create_len(const char *string, size_t length);
rpc_object_t rpc_string_create_with_format(const char *fmt, ...);
rpc_object_t rpc_string_create_with_format_and_arguments(const char *fmt,
    va_list ap);
size_t rpc_string_get_length(rpc_object_t xstring);
const char *rpc_string_get_string_ptr(rpc_object_t xstring);
rpc_object_t rpc_fd_create(int fd);
int rpc_fd_dup(rpc_object_t xfd);
int rpc_fd_get_value(rpc_object_t xfd);

/**
 * Creates a new, empty array of objects.
 *
 * @return Empty array.
 */
rpc_object_t rpc_array_create(void);

/**
 * Creates a new array of objects, optinally populating it with data.
 *
 * @param objects Array of objects to insert
 * @param count Number of iterms in @ref objects
 * @param steal
 * @return
 */
rpc_object_t rpc_array_create_ex(const rpc_object_t *objects, size_t count,
    bool steal);
void rpc_array_set_value(rpc_object_t array, size_t index, rpc_object_t value);
void rpc_array_steal_value(rpc_object_t array, size_t index, rpc_object_t value);
void rpc_array_remove_index(rpc_object_t array, size_t index);
void rpc_array_append_value(rpc_object_t array, rpc_object_t value);
void rpc_array_append_stolen_value(rpc_object_t array, rpc_object_t value);
rpc_object_t rpc_array_get_value(rpc_object_t array, size_t index);
size_t rpc_array_get_count(rpc_object_t array);
bool rpc_array_apply(rpc_object_t array, rpc_array_applier_t applier);
rpc_object_t rpc_array_slice(rpc_object_t array, size_t start, ssize_t len);
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
int rpc_array_get_fd(rpc_object_t array, size_t index);
int rpc_array_dup_fd(rpc_object_t array, size_t index);

#if defined(__linux__)
rpc_shmem_block_t rpc_shmem_alloc(size_t size);
void rpc_shmem_free(rpc_shmem_block_t block);
void *rpc_shmem_block_get_ptr(rpc_shmem_block_t block);
size_t rpc_shmem_block_get_size(rpc_shmem_block_t block);

rpc_object_t rpc_shmem_create(rpc_shmem_block_t block);
rpc_shmem_block_t rpc_shmem_get_block(rpc_object_t obj);
#endif

rpc_object_t rpc_dictionary_create(void);
rpc_object_t rpc_dictionary_create_ex(const char *const *keys,
    const rpc_object_t *values, size_t count, bool steal);
void rpc_dictionary_set_value(rpc_object_t dictionary, const char *key,
    rpc_object_t value);
void rpc_dictionary_steal_value(rpc_object_t dictionary, const char *key,
    rpc_object_t value);
void rpc_dictionary_remove_key(rpc_object_t dictionary, const char *key);
rpc_object_t rpc_dictionary_get_value(rpc_object_t dictionary,
    const char *key);
size_t rpc_dictionary_get_count(rpc_object_t dictionary);
bool rpc_dictionary_apply(rpc_object_t dictionary,
    rpc_dictionary_applier_t applier);
bool rpc_dictionary_has_key(rpc_object_t dictionary, const char *key);
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
int rpc_dictionary_get_fd(rpc_object_t dictionary, const char *key);
int rpc_dictionary_dup_fd(rpc_object_t dictionary, const char *key);

#ifdef __cplusplus
}
#endif

#endif //LIBRPC_OBJECT_H
