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

#include <glib.h>
#include <yaml.h>
#include <rpc/object.h>
#include <rpc/serializer.h>
#include <rpc/typing.h>
#include "internal.h"

static const char *builtin_types[] = {
	"null",
	"bool",
	"uint64",
	"int64",
	"double",
	"date",
	"string",
	"binray",
	"fd",
	"dictionary",
	"array",
	"shmem",
	"error",
	"any",
	NULL
};

static int rpct_read_meta(struct rpct_file *, rpc_object_t);
static struct rpct_member *rpct_read_member(const char *, rpc_object_t,
    struct rpct_type *);
static struct rpct_typei *rpct_instantiate_type(const char *,
    const char *);
static inline bool rpct_type_is_fully_specialized(struct rpct_typei *);
static char *rpct_canonical_type(struct rpct_typei *);
static int rpct_read_type(const char *, const char *, rpc_object_t);
static int rpct_read_func(const char *, const char *, rpc_object_t);
static int rpct_read_file(const char *);
static inline bool rpct_realms_apply(rpct_realm_applier_t applier);
static int rpct_validate_args(struct rpct_function *, rpc_object_t);
static int rpct_validate_return(struct rpct_function *, rpc_object_t);
static int rpct_parse_type(const char *decl, GPtrArray *variables);

static struct rpct_context *context = NULL;

rpc_object_t
rpct_new(const char *decl, const char *realm)
{
	rpc_object_t inst;
	struct rpct_typei *typei;

	typei = rpct_instantiate_type(decl, realm);
	if (typei == NULL)
		return (NULL);

	if (typei->type->clazz == RPC_TYPING_BUILTIN) {
		rpct_type_free(typei);
		return (NULL);
	}

	inst = rpc_dictionary_create();
	inst->ro_typei = typei;

	return (inst);
}

rpct_class_t
rpct_get_class(rpc_object_t instance)
{

	if ((instance == NULL) || (instance->ro_typei == NULL))
		return (NULL);

	return (instance->ro_typei->type->clazz);
}

char *
rpct_get_type(rpc_object_t instance)
{

	if ((instance == NULL) || (instance->ro_typei == NULL))
		return (NULL);

	return (rpct_canonical_type(instance->ro_typei));
}

const char *
rpct_get_value(rpc_object_t instance)
{

}

void
rpct_struct_set_value(rpc_object_t instance, const char *value)
{

}

static inline struct rpct_realm *
rpct_find_realm(const char *realm)
{

	return (g_hash_table_lookup(context->realms, realm));
}

static rpct_type_t
rpct_find_type(const char *realm_name, const char *name)
{
	rpct_realm_t realm;
	rpct_type_t type;
	const char **b;

	for (b = builtin_types; *b != NULL; b++) {
		if (g_strcmp0(name, *b))
			continue;

		type = g_malloc0(sizeof(*type));
		type->name = *b;
		type->clazz = RPC_TYPING_BUILTIN;
		return (type);
	}

	realm = rpct_find_realm(realm_name);
	if (realm == NULL)
		realm = rpct_find_realm("*");

	if (realm == NULL)
		return (NULL);

	return (g_hash_table_lookup(realm->types, name));

}

static int
rpct_read_meta(struct rpct_file *file, rpc_object_t obj)
{
	int ret;

	ret = rpc_object_unpack(obj, "{ssi}",
	    "version", &file->version,
	    "realm", &file->realm,
	    "description", &file->description);

	return (ret > 0 ? 0 : -1);
}

static struct rpct_member *
rpct_read_member(const char *decl, rpc_object_t obj,
    struct rpct_type *type)
{
	struct rpct_member *member;
	const char *typedecl;
	const char *description;

	rpc_object_unpack(obj, "{ss}", obj,
	    "type", &typedecl,
	    "description", &description);

	member = g_malloc0(sizeof(*member));
	member->name = g_strdup(decl);
	member->description = g_strdup(description);
	member->origin = type;
	member->type = rpct_instantiate_type(typedecl, type->realm);

	return (member);
}

static struct rpct_typei *
rpct_instantiate_type(const char *decl, const char *realm)
{
	GError *err = NULL;
	GRegex *regex;
	GMatchInfo *match = NULL;
	GPtrArray *splitvars = NULL;
	struct rpct_type *type;
	struct rpct_typei *ret = NULL;
	struct rpct_typei *subtype;
	char *decltype = NULL;
	char *declvars = NULL;

	regex = g_regex_new(INSTANCE_REGEX, 0, G_REGEX_MATCH_NOTEMPTY, &err);
	if (err != NULL)
		goto error;

	if (!g_regex_match(regex, decl, 0, &match))
		goto error;

	if (g_match_info_get_match_count(match) < 1)
		goto error;

	decltype = g_match_info_fetch(match, 1);

	type = rpct_find_type(realm, decltype);
	if (type == NULL)
		goto error;

	ret = g_malloc0(sizeof(*ret));
	ret->type = type;
	ret->specializations = g_ptr_array_new();
	ret->constraints = type->constraints;

	if (type->generic) {
		declvars = g_match_info_fetch(match, 3);
		if (declvars == NULL)
			goto error;

		splitvars = g_ptr_array_new();

		rpct_parse_type(declvars, splitvars);
		if (splitvars->len != type->generic_vars->len)
			goto error;

		for (int i = 0; i < splitvars->len; i++) {
			subtype = rpct_instantiate_type(
			    g_ptr_array_index(splitvars, i), realm);
			g_ptr_array_add(ret->specializations, subtype);
		}
	}

	goto end;

error:	if (ret != NULL) {
		rpct_type_free(ret);
		ret = NULL;
	}

end:	g_regex_unref(regex);

	if (err != NULL)
		g_error_free(err);

	if (match != NULL)
		g_match_info_free(match);

	if (splitvars != NULL)
		g_ptr_array_free(splitvars, true);

	if (decltype != NULL)
		g_free(decltype);

	if (declvars != NULL)
		g_free(declvars);

	return (ret);
}

void
rpct_type_free(struct rpct_typei *inst)
{

	if (inst->specializations != NULL) {
		for (int i = 0; i < inst->specializations->len; i++) {
			rpct_type_free(
			    g_ptr_array_index(inst->specializations, i));
		}
	}
	g_free(inst);
}

static inline bool
rpct_type_is_fully_specialized(struct rpct_typei *inst)
{
	struct rpct_typei *s;

	if (inst->specializations == NULL)
		return (true);

	for (guint i = 0; i < inst->specializations->len; i++) {
		s = g_ptr_array_index(inst->specializations, i);
		if (s == NULL || s->proxy)
			return (false);
	}

	return (true);
}

static inline bool
rpct_type_is_compatible(struct rpct_typei *decl, struct rpct_typei *type)
{

	/* Types from different realms are always incompatible */
	/* XXX global realm */
	if (g_strcmp0(decl->type->realm, type->type->realm) != 0)
		return (false);


}

/**
 *
 * "int, string, Struct2<bool, int>"
 *
 * @param decl
 * @param variables
 * @return
 */
static int
rpct_parse_type(const char *decl, GPtrArray *variables)
{
	int istart = 0;
	int nesting = 0;
	int groups = 0;

	for (int i = 0; i < strlen(decl); i++) {
		switch (decl[i]) {
		case '<':
			nesting++;
			break;

		case '>':
			nesting--;
			break;

		case ',':
			if (nesting == 0) {
				groups++;
				g_ptr_array_add(variables, g_strndup(
			 	   &decl[istart], (gsize)(i - istart)));
			}
			break;

		default:
			continue;
		}
	}

	return (groups);
}

static char *
rpct_canonical_type(struct rpct_typei *typei)
{
	GString *ret = g_string_new(typei->type->name);

	if (!typei->type->generic)
		return (g_string_free(ret, false));

	g_string_append(ret, "<");

	for (guint i = 0; i < typei->specializations->len; i++) {
		struct rpct_typei *subtype;
		char *substr;

		subtype = g_ptr_array_index(typei->specializations, i);
		substr = rpct_canonical_type(subtype);
		g_string_append(ret, substr);
		g_free(substr);

		if (i < typei->specializations->len)
			g_string_append(ret, ",");
	}

	g_string_append(ret, ">");
	return (g_string_free(ret, false));
}

static int
rpct_read_type(const char *realm, const char *decl, rpc_object_t obj)
{
	struct rpct_type *type;
	struct rpct_type *parent = NULL;
	const char *inherits = NULL;
	const char *description = NULL;
	const char *decltype, *declname, *declvars;
	GError *err = NULL;
	GRegex *regex;
	GMatchInfo *match;
	rpc_object_t members = NULL;

	rpc_object_unpack(obj, "{ssv}",
	    "inherits", &inherits,
	    "description", &description,
	    "members", &members);

	if (inherits != NULL) {
		parent = rpct_find_type(realm, inherits);
		if (parent == NULL)
			return (-1);
	}

	regex = g_regex_new(TYPE_REGEX, 0, G_REGEX_MATCH_NOTEMPTY, &err);
	if (err != NULL) {
		g_error_free(err);
		return (-1);
	}

	if (!g_regex_match(regex, decl, 0, &match)) {
		g_regex_unref(regex);
		return (-1);
	}

	if (g_match_info_get_match_count(match) < 2) {
		g_regex_unref(regex);
		g_match_info_free(match);
		return (-1);
	}

	decltype = g_match_info_fetch(match, 1);
	declname = g_match_info_fetch(match, 2);
	declvars = g_match_info_fetch(match, 3);

	type = g_malloc0(sizeof(*type));
	type->name = g_strdup(declname);
	type->parent = parent;
	type->members = g_hash_table_new(g_str_hash, g_str_equal);
	type->constraints = g_hash_table_new(g_str_hash, g_str_equal);
	type->description = g_strdup(description);

	if (g_strcmp0(decltype, "struct") == 0)
		type->clazz = RPC_TYPING_STRUCT;

	else if (g_strcmp0(decltype, "union") == 0)
		type->clazz = RPC_TYPING_UNION;

	else if (g_strcmp0(decltype, "enum") == 0)
		type->clazz = RPC_TYPING_ENUM;

	else if (g_strcmp0(decltype, "type") == 0)
		type->clazz = RPC_TYPING_TYPEDEF;

	else
		g_assert_not_reached();

	if (declvars)
		rpct_parse_type(declvars, type->generic_vars);

	/* Pull inherited members (if any) */
	if (parent != NULL) {
		GHashTableIter iter;
		gpointer key;
		gpointer value;

		g_hash_table_iter_init(&iter, parent->members);
		while (g_hash_table_iter_next(&iter, &key, &value))
			g_hash_table_insert(type->members, key, value);
	}

	/* Read member list */
	if (members != NULL) {
		rpc_dictionary_apply(members, ^(const char *key, rpc_object_t value) {
			struct rpct_member *m;

			m = rpct_read_member(key, value, type);
			if (m != NULL) {
				g_hash_table_insert(type->members, g_strdup(key), m);
				return ((bool)true);
			}

			/* XXX handle error */
			return ((bool)false);
		});
	}

	g_hash_table_insert(context->types, g_strdup(declname), type);
	return (0);
}

static int
rpct_read_func(const char *realm, const char *decl, rpc_object_t obj)
{
	struct rpct_function *func;
	GError *err = NULL;
	GRegex *regex;
	GMatchInfo *match;
	const char *name;
	const char *description;
	rpc_object_t args;
	rpc_object_t returns;

	rpc_object_unpack(obj, "{ssvv}",
	    "description", &description,
	    "arguments", &args,
	    "return", &returns);

	regex = g_regex_new(FUNC_REGEX, 0, G_REGEX_MATCH_NOTEMPTY, &err);
	if (err != NULL) {
		g_error_free(err);
		g_regex_unref(regex);
		return (-1);
	}

	if (!g_regex_match(regex, decl, 0, &match)) {
		g_regex_unref(regex);
		return (-1);
	}

	if (g_match_info_get_match_count(match) < 1) {
		g_regex_unref(regex);
		g_match_info_free(match);
		return (-1);
	}

	name = g_match_info_fetch(match, 1);
	func = g_malloc0(sizeof(*func));
	func->name = g_strdup(name);

	if (args != NULL) {
		rpc_array_apply(args, ^(size_t idx, rpc_object_t i) {
			return ((bool)true);
		});
	}

}

static int
rpct_read_file(const char *path)
{
	struct rpct_file *file;
	struct rpct_realm *realm;
	char *contents;
	size_t length;
	rpc_object_t obj;
	GError *err = NULL;

	if (!g_file_get_contents(path, &contents, &length, &err)) {
		/* XXX */
		return (-1);
	}

	obj = rpc_serializer_load("yaml", contents, length);
	g_free(contents);

	if (obj == NULL)
		return (-1);

	file = g_malloc0(sizeof(*file));
	rpct_read_meta(file, rpc_dictionary_get_value(obj, "meta"));

	if (rpct_find_realm(file->realm) == NULL) {
		realm = g_malloc0(sizeof(*realm));
		realm->name = g_strdup(file->realm);
		realm->types= g_hash_table_new(g_str_hash, g_str_equal);
		g_hash_table_insert(context->realms, (gpointer)realm->name,
		    realm);
	}

	rpc_dictionary_apply(obj, ^(const char *key, rpc_object_t value) {
		if (g_strcmp0(key, "meta") == 0)
			return ((bool)true);

		if (g_str_has_prefix(key, "function")) {
			rpct_read_func(file->realm, key, value);
			return ((bool)true);
		}

		rpct_read_type(file->realm, key, value);
		return ((bool)true);
	});

	rpc_release(obj);
	return (0);
}

static int
rpct_validate_obj(struct rpct_typei *typei, rpc_object_t obj)
{

}

static int
rpct_validate_args(struct rpct_function *func, rpc_object_t args)
{

	rpc_array_apply(args, ^(size_t idx, rpc_object_t i) {
		return ((bool)true);
	});
}

static int
rpct_validate_return(struct rpct_function *func, rpc_object_t result)
{

}

static inline bool
rpct_realms_apply(rpct_realm_applier_t applier)
{
	GHashTableIter iter;
	rpct_realm_t value;
	char *key;
	bool flag = false;

	g_hash_table_iter_init(&iter, context->realms);
	while (g_hash_table_iter_next(&iter, (gpointer *)&key,
	    (gpointer *)&value))
		if (!applier(value)) {
			flag = true;
			break;
		}

	return (flag);
}

int
rpct_init(void)
{

	context = g_malloc0(sizeof(*context));
	context->files = g_hash_table_new(g_str_hash, g_str_equal);
	context->realms = g_hash_table_new(g_str_hash, g_str_equal);
	context->types = g_hash_table_new(g_str_hash, g_str_equal);
	return (0);
}

int
rpct_load_types(const char *path)
{

	return (rpct_read_file(path));
}

const char *
rpct_type_get_name(rpct_type_t type)
{

	return (type->name);
}

const char *
rpct_type_get_realm(rpct_type_t type)
{

	return (type->realm);
}

const char *
rpct_type_get_description(rpct_type_t type)
{

	return (type->description);
}

rpct_type_t
rpct_type_get_parent(rpct_type_t type)
{

	return (type->parent);
}

bool
rpct_type_is_generic(rpct_type_t type)
{

	return (type->generic);
}

const char *
rpct_member_get_name(rpct_member_t member)
{

	return (member->name);
}

const char *
rpct_member_get_description(rpct_member_t member)
{

	return (member->description);
}

bool
rpct_types_apply(rpct_type_applier_t applier)
{

	return (rpct_realms_apply(^(rpct_realm_t realm) {
		GHashTableIter iter;
		char *key;
		rpct_type_t value;

		g_hash_table_iter_init(&iter, realm->types);
		while (g_hash_table_iter_next(&iter, (gpointer *)&key,
		    (gpointer *)&value))
			if (!applier(value))
				return ((bool)false);

		return ((bool)true);
	}));
}

bool
rpct_members_apply(rpct_type_t type, rpct_member_applier_t applier)
{
	GHashTableIter iter;
	char *key;
	struct rpct_member *value;
	bool flag = false;

	g_hash_table_iter_init(&iter, type->members);
	while (g_hash_table_iter_next(&iter, (gpointer *)&key,
	    (gpointer *)&value))
		if (!applier(value)) {
			flag = true;
			break;
		}

	return (flag);
}
