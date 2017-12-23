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
 */


#ifndef LIBRPC_QUERY_H
#define LIBRPC_QUERY_H

#include <rpc/object.h>

/**
 * @file query.h
 *
 * Object query API
 */

/**
 * Definition of the query iterator structure holding data about the state
 * of a query. Its contents are implementation detail, but the structure itself
 * has to be freed after use using rpc_query_iter_free function.
 */
struct rpc_query_iter;

/**
 * Definition of rpc_query_iter pointer type.
 */
typedef struct rpc_query_iter *rpc_query_iter_t;

/**
 * Definition of query callback block type.
 *
 * Query functions are checking if that block is defined within rpc_query_params
 * structure provided to the query function - if so, then body of the callback
 * is being executed for each of the elements matching query, before yielding
 * them as results. Output of the callback block becomes effectively
 * the output of a query function.
 *
 * Keep in mind that returning NULL from the callback block is considered
 * as skipping currently processed chunk of the result - query function won't
 * return NULL, but look for the next matching element instead.
 */
typedef _Nullable rpc_object_t (^rpc_query_cb_t)(_Nonnull rpc_object_t object);

/**
 * Converts function pointer to an rpc_query_cb_t block type.
 */
#define	RPC_QUERY_CB(_fn, _arg)						\
	^(rpc_object_t _object) {					\
                return ((rpc_object_t)_fn(_arg, _object));		\
        }

/**
 * Query parameters structure definition.
 *
 * Following options could be specified:
 * - single (boolean) - return only first matching element.
 * - count (boolean) - return only the number of matching elements.
 * - offset (unsigned int) - skip specified number of matching elements before
 *   yielding the first result.
 * - limit (unsigned int) - yield no more than a specified number of matching
 *   elements.
 * - sort (rpc_array_cmp_t) - sort the input array before yielding the
 *   results - the sorting function is taking a rpc_array_cmp_t block as
 *   an argument to figure out relations between input an array's elements
 *   (a > b, a = b, a < b).
 * - reverse (boolean) - reverse the order of elements of an input array
 *   (always done after eventual sorting) - be aware that this operation
 *   has to create an intermediate object to store elements in the reversed
 *   order, so it might be expensive for large data sets.
 * - callback (rpc_query_cb_t) - for each of the matching elements run
 *   a callback function first - query will return an RPC object returned
 *   by a callback function, but will skip it if a callback function
 *   returns NULL instead of an RPC object.
 */
struct rpc_query_params {
	bool 				single;
	bool				count;
	uint64_t 			offset;
	uint64_t 			limit;
	bool				reverse;
	_Nullable rpc_array_cmp_t	sort;
	_Nullable rpc_query_cb_t	callback;
};

/**
 * Definition of rpc_query_params pointer type.
 */
typedef struct rpc_query_params *rpc_query_params_t;

/**
 * The function follows a given "key1.key2.0.key3.1" like path to find the last
 * element in the path inside of the provided object and return it.
 *
 * If the function cannot resolve the whole path (i.e. some of intermediate
 * container objects does not have the required key specified),
 * then a provided default value is returned.
 *
 * @param object Object to perform the lookup on.
 * @param path Search path - '.' character is required between each key/idx
 * pair.
 * @param default_val Default value to be returned if desired object couldn't
 * be found - nullable.
 * @return Found RPC object or the default value.
 */
_Nullable rpc_object_t rpc_query_get(_Nonnull rpc_object_t object,
    const char *_Nonnull path, _Nullable rpc_object_t default_val);

/**
 * The function follows a given "key1.key2.0.key3.1" like path to find the last
 * element in the path inside of the provided object and set it to a given
 * value.
 *
 * If the provided path does not match a given object, then library error
 * is set.
 *
 * @param object Object to perform the lookup on.
 * @param path Path to the object to be set - '.' character is required between
 * each key/idx pair.
 * @param value Value to be set.
 * @param steal Boolean flag - if set, then the function does not increase
 * refcount of a set value.
 */
void rpc_query_set(_Nonnull rpc_object_t object, const char *_Nonnull path,
    _Nullable rpc_object_t value,
    bool steal);

/**
 * The function follows a given "key1.key2.0.key3.1" like path to find the last
 * element in the path inside of the provided object and delete it.
 *
 * If the provided path does not match a given object, then library error
 * is set.
 *
 * @param object Object to perform the lookup on.
 * @param path Path to the object to be deleted - '.' character is required
 * between each key/idx pair.
 */
void rpc_query_delete(_Nonnull rpc_object_t object, const char *_Nonnull path);

/**
 * The function follows a given "key1.key2.0.key3.1" like path to check whether
 * or not a given object does have an object under a given path set.
 *
 * The function returns the boolean result of that check.
 *
 * @param object Object to perform the lookup on.
 * @param path Path to the object to be found - '.' character is required
 * between each key/idx pair.
 * @return Boolean result of the check.
 */
bool rpc_query_contains(_Nonnull rpc_object_t object, const char *_Nonnull path);

/**
 * Performs a query operation on a given object.
 * Source object has to be an RPC object of array type, but it can contain
 * any sequence of internal data objects.
 *
 * The function immediately returns iterator object without performing
 * any initial operations on the input data. To get actual data, user has to use
 * the rpc_query_next function, providing iterator as an argument.
 *
 * Params arguments defines query runtime parameters - precise definition
 * of possible options could be found in rpc_query_params documentation.
 *
 * Each of the processed array's elements is checked against the rules.
 * If it matches the rules, then it can be yielded from the query.
 *
 * Rules are describes as 2 or 3 element arrays. 3 element arrays describe
 * logic operators and 2 element arrays describe field operators.
 *
 * First argument of logic operator describes a tested path within a processed
 * object (i.e "a.b.c.0"), second argument is a string describing the logic
 * operator itself and the last one is the value provided externally
 * by the user as a second operand of a logic comparison.
 *
 * There are following logic operators allowed
 * (B - right operand, A - left operand):
 * - = - equal - A = B
 * - != - not equal - A != B
 * - > - greater - A > B
 * - < - smaller - A < B
 * - >= - greater or equal - A >= B
 * - <= - smaller or equal - A <= B
 * - ~ - regular expression (PCRE) - A matches B rules
 * - in - value in array - A in B (when B is an array) or B in A
 *   (when A is an array)
 * - nin - value not in array - A not in B (when B is an array) or B not in A
 *   (when A is an array)
 * - contains - the same as in
 * - ncontains - the same as nin
 * - match - fnmatch() (unix filename pattern matching) - fnmatch(B, A, 0) == 0
 *
 * Field operators are used for chaining arrays of logic operators and defining
 * logic relations between them: i.e. ["and", [["A", ">", 0], ["A", "<", 4]]]
 * There are following field operators allowed:
 * - and
 * - or
 * - nor
 *
 * Example of a complex rule:
 * ["or", [["a.b.0.c", "=", 1], ["and", [["a.d", ">", 2], ["a.d", "<", 4]]]]]
 *
 * @param object Object to be queried.
 * @param params Query parameters.
 * @param rules Query rules.
 * @return Query iterator.
 */
_Nullable rpc_query_iter_t rpc_query(_Nonnull rpc_object_t object,
    _Nullable rpc_query_params_t params, _Nullable rpc_object_t rules);

/**
 * Performs a query operation on a given object.
 *
 * The function works exactly the same as the rpc_query function, but does not
 * require the user to provide rules as an assembled RPC object.
 * Instead it assembles query rules on the fly using the rpc_object_pack
 * function syntax.
 *
 * @param object Object to be queried.
 * @param params Query parameters.
 * @param rules_fmt Rules rpc_object_pack like format string.
 * @param ... Variable length list of arguments to be assembled as query rules.
 * @return Query iterator.
 */
_Nullable rpc_query_iter_t rpc_query_fmt(_Nonnull rpc_object_t object,
    _Nonnull rpc_query_params_t params, const char *_Nonnull rules_fmt, ...);

/**
 * Checks if a given RPC object does match a provided object representing
 * query rules (the same format as in the rpc_query function case).
 *
 * If so, then the function does return the object increasing its reference
 * count, otherwise it returns NULL.
 *
 * @param object Object to be checked against a given set of rules.
 * @param rules Set of query-like rules.
 * @return The object itself if is matches the rules, otherwise NULL.
 */
_Nullable rpc_object_t rpc_query_apply(_Nonnull rpc_object_t object,
    _Nonnull rpc_object_t rules);

/**
 * Yields the next RPC object matching params and rules stored within
 * iterator structure.
 *
 * If further iteration is still possible, returns true, otherwise false.
 *
 * The function stores the current result in chunk argument. Chunk could be set
 * to NULL only if there's internal error condition, there are no data matching
 * given rules and params in the source object or when the user tries to iterate
 * again over previously finished iterator structure.
 *
 * Function automatically increases reference count of the returned RPC object.
 *
 * @param iter Iterator structure.
 * @param chunk Pointer to be set to the next result.
 * @return "Continue iteration" boolean flag.
 */
bool rpc_query_next(_Nonnull rpc_query_iter_t iter,
    _Nonnull rpc_object_t *_Nullable chunk);

/**
 * Releases internal contents of rpc query iterator structure and then
 * the structure itself.
 *
 * @param iter Structure to be freed.
 */
void rpc_query_iter_free(_Nonnull rpc_query_iter_t iter);

#endif //LIBRPC_QUERY_H
