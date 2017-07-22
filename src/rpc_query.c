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

#include <rpc/query.h>
#include <string.h>
#include <glib.h>
#include <stdlib.h>
#include <errno.h>
#include <fnmatch.h>
#include "internal.h"

static bool eval_rule(rpc_object_t obj, rpc_object_t rule);

static rpc_object_t
rpc_query_steal_get(rpc_object_t object, const char *path,
		    rpc_object_t default_val)
{
	char *split_path = g_strdup(path);
	char *token;
	rpc_object_t leaf = object;
	rpc_object_t retval = default_val;
	bool error = false;

	token = strtok(split_path, ".");
	while (token != NULL) {
		switch(rpc_get_type(leaf)) {
			case RPC_TYPE_DICTIONARY:
				leaf = rpc_dictionary_get_value(leaf, token);
				break;

			case RPC_TYPE_ARRAY:
				leaf = rpc_array_get_value(leaf, (size_t)atoi(token));
				break;

			default:
				retval = NULL;
				error = true;
				break;
		}

		if (error)
			break;

		token = strtok(NULL, ".");
	}

	if ((!error) && (leaf != NULL))
		retval = leaf;

	g_free(split_path);

	return (retval);
}

static rpc_object_t
rpc_query_get_parent(rpc_object_t object, const char *path,
    const char **child, rpc_object_t default_val)
{
	char *get_path;
	size_t get_path_len;
	rpc_object_t parent;
	char *last_segment = g_strrstr(path, ".") + 1;

	get_path_len = strlen(path) - strlen(last_segment) - 1;
	get_path = g_strndup(path, get_path_len);

	parent = rpc_query_steal_get(object, (const char *)get_path, default_val);

	g_free(get_path);
	*child = (const char *)last_segment;
	return (parent);
}

static bool
eval_logic_and(rpc_object_t obj, rpc_object_t lst)
{
	__block bool result = false;

	if (rpc_get_type(lst) != RPC_TYPE_ARRAY)
		return (false);

	rpc_array_apply(lst, ^(size_t idx __unused, rpc_object_t v) {
		result = eval_rule(obj, v);
		return ((bool)result);
	});

	return (result);
}

static bool
eval_logic_or(rpc_object_t obj, rpc_object_t lst) {
	__block bool result = false;

	if (rpc_get_type(lst) != RPC_TYPE_ARRAY)
		return (false);

	rpc_array_apply(lst, ^(size_t idx __unused, rpc_object_t v) {
	    result = eval_rule(obj, v);
	    return ((bool)!result);
	});

	return (result);
}

static bool
eval_logic_nor(rpc_object_t obj, rpc_object_t lst) {
	__block bool result = false;

	if (rpc_get_type(lst) != RPC_TYPE_ARRAY)
		return (false);

	rpc_array_apply(lst, ^(size_t idx __unused, rpc_object_t v) {
	    result = !eval_rule(obj, v);
	    return ((bool)result);
	});

	return (result);
}

static bool
eval_logic_operator(rpc_object_t obj, rpc_object_t rule)
{
	const char *op;
	rpc_object_t lst;

	if (rpc_get_type(rule) != RPC_TYPE_ARRAY)
		return (false);

	op = rpc_array_get_string(rule, 0);
	lst = rpc_array_get_value(rule, 1);

	if (!g_strcmp0(op, "or")) {
		return (eval_logic_or(obj, lst));
	}

	if (!g_strcmp0(op, "and")) {
		return (eval_logic_and(obj, lst));
	}

	if (!g_strcmp0(op, "nor")) {
		return (eval_logic_nor(obj, lst));
	}

	return (false);
}

static bool
op_in(rpc_object_t o1, rpc_object_t o2)
{
	if (rpc_get_type(o2) == RPC_TYPE_ARRAY) {
		return (rpc_array_contains(o2, o1));
	}

	if (rpc_get_type(o1) == RPC_TYPE_ARRAY) {
		return (rpc_array_contains(o1, o2));
	}

	return (false);
}

static bool
eval_field_operator(rpc_object_t obj, rpc_object_t rule)
{
	const char *left;
	const char *op;
	rpc_object_t right;
	rpc_object_t item;

	if (rpc_get_type(rule) != RPC_TYPE_ARRAY)
		return (false);

	left = rpc_array_get_string(rule, 0);
	op = rpc_array_get_string(rule, 1);
	right = rpc_array_get_value(rule, 2);

	item = rpc_query_steal_get(obj, left, NULL);

	if (!g_strcmp0(op, "=")) {
		return (rpc_equal(item, right));

	}

	if (!g_strcmp0(op, "!=")) {
		return (rpc_cmp(item, right) != 0);

	}

	if (!g_strcmp0(op, ">")) {
		return (rpc_cmp(item, right) > 0);

	}

	if (!g_strcmp0(op, "<")) {
		return (rpc_cmp(item, right) < 0);

	}

	if (!g_strcmp0(op, ">=")) {
		return (rpc_cmp(item, right) >= 0);

	}

	if (!g_strcmp0(op, "<=")) {
		return (rpc_cmp(item, right) <= 0);

	}

	if (!g_strcmp0(op, "~")) {
		if (rpc_get_type(right) != RPC_TYPE_STRING)
			return (false);
		if (rpc_get_type(item) != RPC_TYPE_STRING)
			return (false);

		return ((bool)g_regex_match_simple(
		    rpc_string_get_string_ptr(right),
		    rpc_string_get_string_ptr(item), 0, 0));

	}

	if ((!g_strcmp0(op, "in")) || (!g_strcmp0(op, "contains"))) {
		return (op_in(item, right));

	}

	if ((!g_strcmp0(op, "nin")) || (!g_strcmp0(op, "ncontains"))) {
		return (!op_in(item, right));

	}

	if (!g_strcmp0(op, "match")) {
		if (rpc_get_type(right) != RPC_TYPE_STRING)
			return (false);
		if (rpc_get_type(item) != RPC_TYPE_STRING)
			return (false);

		return (fnmatch(rpc_string_get_string_ptr(right),
		    rpc_string_get_string_ptr(item), 0) == 0);
	}

	return (false);
}


static bool
eval_rule(rpc_object_t obj, rpc_object_t rule)
{
	if (rpc_get_type(rule) != RPC_TYPE_ARRAY)
		return (false);

	switch (rule->ro_value.rv_list->len) {
	case 2:
		return (eval_logic_operator(obj, rule));
	case 3:
		return (eval_field_operator(obj, rule));
	default:
		return (false);
	}
}

static rpc_object_t
rpc_query_steal_apply(rpc_object_t object, rpc_object_t rules)
{
	__block bool fail = false;

	if (object == NULL)
		return (NULL);

	if (rules == NULL)
		return (NULL);

	if (rpc_get_type(rules) != RPC_TYPE_ARRAY)
		return (NULL);

	rpc_array_apply(rules, ^(size_t idx __unused, rpc_object_t v) {
		if (!eval_rule(object, v)) {
			fail = true;
			return ((bool)false);
		}
	    	return (bool)true;
	});

	return (fail ? NULL : object);
}

static rpc_object_t
rpc_query_find_next(rpc_query_iter_t iter)
{
	rpc_object_t current = NULL;
	rpc_object_t result = NULL;

	do {
		current = rpc_array_get_value(iter->source, iter->cur_idx);
		iter->cur_idx++;
		result = rpc_query_steal_apply(current, iter->rules);

	} while ((current != NULL) && (result == NULL));

	if (iter->params->limit > 0) {
		if ((result != NULL) && (iter->initialized)) {
			if (iter->limit_cnt < iter->params->limit)
				iter->limit_cnt++;
			else
				result = NULL;
		}
	}

	return (result);
}

rpc_object_t
rpc_query_get(rpc_object_t object, const char *path, rpc_object_t default_val)
{
	rpc_object_t retval;

	retval = rpc_query_steal_get(object, path, default_val);

	if (retval != NULL)
		rpc_retain(retval);

	return (retval);
}

void
rpc_query_set(rpc_object_t object, const char *path, rpc_object_t value,
    bool steal)
{
	rpc_object_t container;
	const char *child;

	container = rpc_query_get_parent(object, path, &child, NULL);
	if (container == NULL) {
		rpc_set_last_error(ENOENT, "SET: Parent object not found.",
		    NULL);
		return;
	}

	if (child == NULL) {
		rpc_set_last_error(ENOENT,
		    "SET: Path too short - specify target for the value.",
		    NULL);
		return;
	}

	switch(rpc_get_type(container)) {
	case RPC_TYPE_DICTIONARY:
		if (steal)
			rpc_dictionary_steal_value(container, child, value);
		else
			rpc_dictionary_set_value(container, child, value);

		break;

	case RPC_TYPE_ARRAY:
		if (steal)
			rpc_array_steal_value(container,
			    (size_t)atoi(child), value);
		else
			rpc_array_set_value(container,
			    (size_t)atoi(child), value);

		break;

	default:
		rpc_set_last_error(EINVAL,
		    "SET: Cannot navigate through non-container types.", NULL);
		break;
	}
}

void
rpc_query_delete(rpc_object_t object, const char *path)
{
	rpc_object_t container;
	const char *child;

	container = rpc_query_get_parent(object, path, &child, NULL);
	if (container == NULL) {
		rpc_set_last_error(ENOENT, "DELETE: Parent object not found.",
		    NULL);
		return;
	}

	if (child == NULL) {
		rpc_set_last_error(ENOENT,
		    "DELETE: Path too short - specify target for the value.",
		    NULL);
		return;
	}

	switch(rpc_get_type(container)) {
	case RPC_TYPE_DICTIONARY:
		rpc_dictionary_remove_key(container, child);
		break;

	case RPC_TYPE_ARRAY:
		rpc_array_remove_index(container, (size_t)atoi(child));
		break;

	default:
		rpc_set_last_error(EINVAL,
		    "DELETE: Cannot navigate through non-container types.",
		    NULL);
		break;
	}
}

bool
rpc_query_contains(rpc_object_t object, const char *path)
{
	rpc_object_t target = rpc_query_steal_get(object, path, NULL);

	return (target != NULL);
}

rpc_query_iter_t
rpc_query(rpc_object_t object, rpc_query_params_t params, rpc_object_t rules)
{
	rpc_query_iter_t iter;
	rpc_query_params_t local_params;

	if (rpc_get_type(object) != RPC_TYPE_ARRAY) {
		rpc_set_last_error(EINVAL, "Query can operate on arrays only",
		    NULL);
		return (NULL);
	}

	iter = (rpc_query_iter_t)g_malloc(sizeof(*iter));

	local_params = (rpc_query_params_t)g_malloc(sizeof(*local_params));

	if (params != NULL)
		*local_params = *params;

	iter->source = object;
	iter->cur_idx = 0;
	iter->params = local_params;
	iter->rules = rules;
	iter->done = false;
	iter->initialized = false;
	iter->limit_cnt = 0;

	rpc_retain(rules);
	rpc_retain(object);
	return (iter);
}

rpc_query_iter_t
rpc_query_fmt(rpc_object_t object, rpc_query_params_t params,
    const char *rules_fmt, ...)
{
	va_list ap;
	rpc_object_t rules;
	rpc_query_iter_t iter;

	va_start(ap, rules_fmt);
	rules = rpc_object_vpack(rules_fmt, ap);
	va_end(ap);

	iter = rpc_query(object, params, rules);
	rpc_release(rules);
	return (iter);
}

rpc_object_t
rpc_query_apply(rpc_object_t object, rpc_object_t rules)
{
	rpc_object_t result;

	if (rpc_get_type(rules) != RPC_TYPE_ARRAY)
		return (NULL);

	result = rpc_query_steal_apply(object, rules);

	if (result != NULL)
		rpc_retain(result);

	return (result);
}

bool
rpc_query_next(rpc_query_iter_t iter, rpc_object_t *chunk)
{
	rpc_object_t temp_obj = NULL;
	uint32_t match_cnt = 0;
	uint32_t i;

	if (iter->done) {
		*chunk = NULL;
		return (false);
	}

	if (!iter->initialized) {
		if (iter->params->sort)
			rpc_array_sort(iter->source, iter->params->sort);

		if (iter->params->reverse) {
			temp_obj = rpc_array_create();
			rpc_array_reverse_apply(iter->source,
			    ^(size_t idx __unused, rpc_object_t v) {
				rpc_array_append_value(temp_obj, v);
				return ((bool) true);
			});

			rpc_release(iter->source);
			iter->source = temp_obj;
		}

		for (i = 0; i < iter->params->offset; i++) {
			temp_obj = rpc_query_find_next(iter);
			if (temp_obj == NULL)
				break;
		}

		iter->initialized = true;
	}

	if (iter->params->count) {
		do {
			temp_obj = rpc_query_find_next(iter);
			match_cnt++;

		} while (temp_obj != NULL);
		iter->done = true;
		*chunk = rpc_uint64_create(match_cnt);
		return (false);
	}

	if (iter->params->single) {
		temp_obj = rpc_query_find_next(iter);
		if (temp_obj != NULL)
			rpc_retain(temp_obj);

		*chunk = temp_obj;
		iter->done = true;
		return (false);
	}

	do {
		temp_obj = rpc_query_find_next(iter);

		if (temp_obj == NULL)
			break;

		if (iter->params->callback)
			temp_obj = iter->params->callback(temp_obj);

	} while (temp_obj == NULL);

	*chunk = temp_obj;

	if (temp_obj == NULL) {
		iter->done = true;
		return (false);
	}

	rpc_retain(temp_obj);
	return (true);
}

void
rpc_query_iter_free(rpc_query_iter_t iter)
{

	rpc_release(iter->rules);
	rpc_release(iter->source);
	g_free(iter->params);
	g_free(iter);
}
