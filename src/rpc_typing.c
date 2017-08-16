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

#define	TYPE_REGEX	"(struct|union|type|enum) (\\w+)(<(.*)>)?"
#define	INSTANCE_REGEX	"(\\w+)(<(.*)>)?"
#define	FUNC_REGEX	"function (\\w+)"

struct rpct_context
{
	GHashTable *		types;
	GHashTable *		files;
	GHashTable *		realms;
};

struct rpct_realm
{
	const char *		name;
	GHashTable *		types;
};

struct rpct_file
{
	const char *		path;
	const char *		realm;
	const char *		description;
	int64_t			version;
	GHashTable *		types;
};

/**
 * An RPC type.
 */
struct rpct_type
{
	rpct_class_t		clazz;
	const char *		realm;
	const char *		name;
	const char *		description;
	struct rpct_type *	parent;
	bool			generic;
	GPtrArray *		generic_vars;
	GHashTable *		members;
	GHashTable *		constraints;
};

/**
 * This structure has two uses. It can hold either:
 * a) An instantiated (specialized or partially specialized) type
 * b) A "proxy" type that refers to parent's type generic variable
 */
struct rpct_typei
{
	bool			proxy;
	struct rpct_type *	type;
	GPtrArray *		specializations;
	GHashTable *		constraints;
};

/**
 *
 */
struct rpct_member
{
	char *			name;
	char *			description;
	struct rpct_typei *	type;
	struct rpct_type *	origin;
	GHashTable *		constraints;
};

struct rpct_function
{
	char *			name;
	char *			description;
	GPtrArray *		arguments;
	struct rpct_typei *	result;

};

struct rpct_instance
{
	struct rpct_typei *	type;
	rpc_object_t		dict;
};

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
    struct rpct_type *);
static inline bool rpct_type_is_fully_specialized(struct rpct_typei *);
static char *rpct_canonical_type(struct rpct_typei *);
static int rpct_read_type(const char *, const char *, rpc_object_t);
static int rpct_read_func(const char *, const char *, rpc_object_t);
static int rpct_read_file(const char *);
static int rpct_validate_args(struct rpct_function *, rpc_object_t);
static int rpct_validate_return(struct rpct_function *, rpc_object_t);

static struct rpct_context *context = NULL;

rpct_instance_t
rpct_new(const char *decl, ...)
{
	struct rpct_instance *inst;
	struct rpct_typei *typei;
	char *canon;

	typei = rpct_instantiate_type(decl, NULL);
	if (typei == NULL)
		return (NULL);

	canon = rpct_canonical_type(typei);
	inst = g_malloc0(sizeof(*inst));
	inst->type = typei;
	inst->dict = rpc_dictionary_create();
	rpc_dictionary_set_string(inst->dict, RPCT_TYPE_FIELD, canon);
	g_free(canon);
	return (inst);
}

rpct_instance_t
rpct_unpack(rpc_object_t obj)
{

}

rpc_object_t
rpct_pack(rpct_instance_t instance)
{

}

void
rpct_free(rpct_instance_t instance)
{

}

rpct_class_t
rpct_get_class(rpct_instance_t instance)
{

}

const char *
rpct_get_type(rpct_instance_t instance)
{

}

const char *
rpct_get_value(rpct_instance_t instance)
{

}

void
rpct_struct_set_value(rpct_instance_t instance, const char *value)
{

}

rpc_object_t
rpct_get_dict(rpct_instance_t instance)
{

}

static inline struct rpct_realm *
rpct_find_realm(const char *realm)
{

	return (g_hash_table_lookup(context->realms, realm));
}

static struct rpct_type *
rpct_find_type(const char *realm_name, const char *name)
{
	struct rpct_realm *realm;
	struct rpct_type *type;
	const char **b;

	for (b = builtin_types; *b != NULL; b++) {
		if (g_strcmp0(name, *b))
			continue;

		type = g_malloc0(sizeof(*type));
		type->name = *b;
		type->clazz = RPC_TYPING_BUILTIN;
		return (type);
	}

	realm = rpct_find_realm("*");
	if (realm != NULL) {

	}

	realm = rpct_find_realm(realm_name);
	if (realm != NULL) {

	}

	return (NULL);
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
	member->type = rpct_instantiate_type(typedecl, type);

	return (member);
}

static struct rpct_typei *
rpct_instantiate_type(const char *decl, struct rpct_type *parent)
{
	GError *err = NULL;
	GRegex *regex;
	GMatchInfo *match;
	struct rpct_type *type;
	struct rpct_typei *ret;
	const char *decltype;
	const char *declvars;

	regex = g_regex_new(INSTANCE_REGEX, 0, G_REGEX_MATCH_NOTEMPTY, &err);
	if (err != NULL) {
		g_error_free(err);
		g_regex_unref(regex);
		return (NULL);
	}

	if (!g_regex_match(regex, decl, 0, &match)) {
		g_regex_unref(regex);
		return (NULL);
	}

	if (g_match_info_get_match_count(match) < 1) {
		g_regex_unref(regex);
		g_match_info_free(match);
		return (NULL);
	}

	type = rpct_find_type(parent->realm, decltype);
	if (type == NULL)
		return (NULL);

	ret = g_malloc0(sizeof(*ret));
	ret->type = type;
	ret->specializations = g_ptr_array_new();

	return (ret);
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
			 	   &decl[istart], i - istart));
			}
			break;
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

void
rpct_types_apply(rpct_type_applier_t applier)
{
	GHashTableIter iter;
	char *key;
	struct rpct_type *value;

	g_hash_table_iter_init(&iter, context->types);
	while (g_hash_table_iter_next(&iter, &key, &value))
		applier(value);
}

void
rpct_members_apply(rpct_type_t type, rpct_member_applier_t applier)
{
	GHashTableIter iter;
	char *key;
	struct rpct_member *value;

	g_hash_table_iter_init(&iter, type->members);
	while (g_hash_table_iter_next(&iter, &key, &value))
		applier(value);
}
