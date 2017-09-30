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
 * DIRECT, INDIRECT\\, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
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

static int rpct_read_meta(struct rpct_file *file, rpc_object_t obj);
static int rpct_lookup_type(const char *realm_name, const char *name,
    const char **decl, rpc_object_t *result);
static struct rpct_type *rpct_find_type(const char *realm_name, const char *name);
static struct rpct_typei *rpct_instantiate_type(const char *decl,
    const char *realm, struct rpct_typei *parent, struct rpct_type *ptype);
static inline bool rpct_type_is_fully_specialized(struct rpct_typei *inst);
static inline struct rpct_typei *rpct_unwind_typei(struct rpct_typei *typei);
static char *rpct_canonical_type(struct rpct_typei *typei);
static struct rpct_member *rpct_read_member(const char *decl,
    rpc_object_t obj, struct rpct_type *type);
static struct rpct_member *rpct_read_enum_member(const char *decl,
    rpc_object_t obj, struct rpct_type *type);
static int rpct_read_type(const char *realm, const char *decl,
    rpc_object_t obj);
static int rpct_read_func(const char *realm, const char *decl,
    rpc_object_t obj);
static int rpct_read_file(const char *path);
static inline bool rpct_realms_apply(rpct_realm_applier_t applier);
static bool rpct_validate_instance(rpc_object_t obj, struct rpct_error_context *ctx);
static bool rpct_validate_args(struct rpct_function *func, rpc_object_t args);
static bool rpct_validate_return(struct rpct_function *func,
    rpc_object_t result);
static int rpct_parse_type(const char *decl, GPtrArray *variables);
static void rpct_constraint_free(struct rpct_constraint *constraint);
static void rpct_add_error(struct rpct_error_context *ctx, const char *fmt, ...);
static void rpct_derive_error_context(struct rpct_error_context *newctx,
    struct rpct_error_context *oldctx, const char *name);
static void rpct_release_error_context(struct rpct_error_context *ctx);


static struct rpct_context *context = NULL;
static struct rpct_class_handler class_handlers[] = {
	{
		.id = RPC_TYPING_STRUCT,
		.name = "struct",
		.member_fn = rpct_read_member,
		.validate_fn = rpct_validate_struct
	},
	{
		.id = RPC_TYPING_UNION,
		.name = "union",
		.member_fn = rpct_read_member,
		.validate_fn = rpct_validate_union
	},
	{
		.id = RPC_TYPING_TYPEDEF,
		.name = "typedef",
		.member_fn = NULL,
		.validate_fn = NULL
	},
	{
		.id = RPC_TYPING_ENUM,
		.name = "enum",
		.member_fn = rpct_read_enum_member,
		.validate_fn = rpct_validate_enum
	},
	{
		.id = RPC_TYPING_BUILTIN,
		.name = "builtin",
		.member_fn = NULL,
		.validate_fn = rpct_validate_builtin
	},
	{}
};

static const char *builtin_types[] = {
	"null",
	"bool",
	"uint64",
	"int64",
	"double",
	"date",
	"string",
	"binary",
	"fd",
	"dictionary",
	"array",
	"shmem",
	"error",
	"any",
	NULL
};


rpct_typei_t
rpct_new_typei(const char *decl)
{

	return (rpct_instantiate_type(decl, context->global_realm, NULL, NULL));
}

rpc_object_t
rpct_new(const char *decl, const char *realm, rpc_object_t object)
{
	struct rpct_typei *typei;

	if (realm == NULL)
		realm = context->global_realm;

	typei = rpct_instantiate_type(decl, realm, NULL, NULL);
	if (typei == NULL)
		return (NULL);

	return (rpct_newi(typei, object));
}

rpc_object_t
rpct_newi(rpct_typei_t typei, rpc_object_t object)
{
	if (object == NULL)
		object = rpc_dictionary_create();

	object->ro_typei = typei;
	return (object);
}

rpct_class_t
rpct_get_class(rpc_object_t instance)
{

	return (instance->ro_typei->type->clazz);
}

rpct_type_t rpct_get_type(const char *name)
{

	return rpct_find_type(context->global_realm, name);
}

rpct_typei_t
rpct_get_typei(rpc_object_t instance)
{

	if ((instance == NULL) || (instance->ro_typei == NULL))
		return (NULL);

	return (instance->ro_typei);
}

rpc_object_t
rpct_get_value(rpc_object_t instance)
{

	if ((instance == NULL) || (instance->ro_typei == NULL))
		return (NULL);

	if (rpc_get_type(instance) != RPC_TYPE_DICTIONARY)
		return (NULL);

	return (rpc_dictionary_get_value(instance, RPCT_VALUE_FIELD));
}

void
rpct_struct_set_value(rpc_object_t instance, const char *value)
{

	if ((instance == NULL) || (instance->ro_typei == NULL))
		return;

	if (rpc_get_type(instance) != RPC_TYPE_DICTIONARY)
		return;

	rpc_dictionary_set_string(instance, RPCT_VALUE_FIELD, value);
}

static inline struct rpct_realm *
rpct_find_realm(const char *realm)
{

	return (g_hash_table_lookup(context->realms, realm));
}

static struct rpct_type *
rpct_find_type(const char *realm_name, const char *name)
{
	rpct_realm_t realm;
	rpct_type_t type = NULL;

	realm = rpct_find_realm(realm_name);
	type = g_hash_table_lookup(realm->types, name);

	if (type == NULL) {
		realm = rpct_find_realm("*");
		type = g_hash_table_lookup(realm->types, name);
	}

	if (type == NULL) {
		const char *decl;
		rpc_object_t obj;

		debugf("type %s::%s not found, trying to look it up",
		    realm_name, name);

		if (rpct_lookup_type(realm_name, name, &decl, &obj) == 0)
			rpct_read_type(realm_name, decl, obj);

		debugf("hopefully %s::%s is loaded now", realm_name, name);

		realm = rpct_find_realm(realm_name);
		type = g_hash_table_lookup(realm->types, name);
		if (type != NULL)
			debugf("successfully chain-loaded %s::%s", realm_name, name);
	}

	return (type);

}

static int
rpct_read_meta(struct rpct_file *file, rpc_object_t obj)
{
	int ret;

	ret = rpc_object_unpack(obj, "{s,s,i}",
	    "version", &file->version,
	    "realm", &file->realm,
	    "description", &file->description);

	return (ret > 0 ? 0 : -1);
}

static struct rpct_member *
rpct_read_member(const char *decl, rpc_object_t obj, struct rpct_type *type)
{
	struct rpct_member *member;
	const char *typedecl = NULL;
	const char *description = "";

	rpc_object_unpack(obj, "{s,s}",
	    "type", &typedecl,
	    "description", &description);

	member = g_malloc0(sizeof(*member));
	member->name = g_strdup(decl);
	member->description = description != NULL ? g_strdup(description) : NULL;
	member->origin = type;
	member->type = rpct_instantiate_type(typedecl, type->realm, NULL, type);
	member->constraints = g_hash_table_new_full(g_str_hash, g_str_equal,
	    g_free, (GDestroyNotify)rpct_constraint_free);

	return (member);
}

static struct rpct_member *
rpct_read_enum_member(const char *decl, rpc_object_t obj, struct rpct_type *type)
{
	struct rpct_member *member;
	const char *description = "";

	rpc_object_unpack(obj, "{s}", "description", &description);

	member = g_malloc0(sizeof(*member));
	member->name = g_strdup(decl);
	member->description = g_strdup(description);
	member->origin = type;
	return (member);
}

static struct rpct_typei *
rpct_instantiate_type(const char *decl, const char *realm,
    struct rpct_typei *parent, struct rpct_type *ptype)
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
	int found_proxy_type = -1;

	debugf("instantiating type %s in realm %s", decl, realm);

	regex = g_regex_new(INSTANCE_REGEX, 0, G_REGEX_MATCH_NOTEMPTY, &err);
	if (err != NULL)
		goto error;

	if (!g_regex_match(regex, decl, 0, &match))
		goto error;

	if (g_match_info_get_match_count(match) < 1)
		goto error;

	decltype = g_match_info_fetch(match, 1);
	type = rpct_find_type(realm, decltype);
	if (type == NULL) {
		struct rpct_typei *cur = parent;

		debugf("type %s::%s not found, maybe it's a generic variable",
		    realm, decltype);

		while (cur != NULL) {
			/* Maybe it's a type variable? */
			if (cur->type->generic) {
				subtype = g_hash_table_lookup(cur->specializations, decltype);
				if (subtype)
					return (subtype);
			}

			cur = cur->parent;
		}

		if (ptype != NULL) {
			/* Maybe it's a type variable? */
			if (ptype->generic) {
				found_proxy_type = rpc_ptr_array_string_index(
				    ptype->generic_vars, decltype);
			}

			if (found_proxy_type != -1) {
				subtype = g_malloc0(sizeof(*subtype));
				subtype->proxy = true;
				subtype->variable = g_strdup(decltype);
				subtype->canonical_form = g_strdup(decltype);
				return (subtype);
			}
		}

		goto error;
	}

	ret = g_malloc0(sizeof(*ret));
	ret->type = type;
	ret->parent = parent;
	ret->specializations = g_hash_table_new_full(g_str_hash, g_str_equal,
	    g_free, (GDestroyNotify)rpct_typei_free);
	ret->constraints = type->constraints;

	if (type->generic) {
		declvars = g_match_info_fetch(match, 3);
		if (declvars == NULL)
			goto error;

		splitvars = g_ptr_array_new();
		rpct_parse_type(declvars, splitvars);

		if (splitvars->len != type->generic_vars->len)
			goto error;

		for (guint i = 0; i < splitvars->len; i++) {
			const char *var = g_ptr_array_index(type->generic_vars, i);
			const char *vartype = g_ptr_array_index(splitvars, i);

			subtype = rpct_instantiate_type(vartype, realm, ret, ptype);

			if (subtype == NULL)
				goto error;

			g_hash_table_insert(ret->specializations, g_strdup(var),
			    subtype);
		}
	}

	ret->canonical_form = rpct_canonical_type(ret);
	goto done;

error:
	if (ret != NULL) {
		rpct_typei_free(ret);
		ret = NULL;
	}

done:
	g_regex_unref(regex);

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

static struct rpct_typei *
rpct_instantiate_member(struct rpct_member *member, struct rpct_typei *parent)
{
	struct rpct_typei *ret;

	ret = rpct_instantiate_type(member->type->canonical_form,
	    member->origin->realm, parent, parent->type);

	return (ret);
}

static void
rpct_type_free(rpct_type_t type)
{

	g_free(type->realm);
	g_free(type->name);
	g_free(type->description);
	g_ptr_array_free(type->generic_vars, true);
	g_hash_table_unref(type->members);
	g_hash_table_unref(type->constraints);
	g_free(type);
}

static void
rpct_file_free(struct rpct_file *file)
{

	g_free(file->path);
	g_free(file->realm);
	g_free(file->description);
	g_hash_table_unref(file->types);
	g_free(file);
}

static void
rpct_realm_free(rpct_realm_t realm)
{

	g_free(realm->name);
	g_hash_table_unref(realm->types);
	g_hash_table_unref(realm->functions);
	g_free(realm);
}

static void
rpct_function_free(struct rpct_function *function)
{

	g_free(function->name);
	g_free(function->realm);
	g_free(function->description);
	g_hash_table_unref(function->arguments);
	if (function->result != NULL)
		rpct_typei_free(function->result);
	g_free(function);
}

static void
rpct_member_free(struct rpct_member *member)
{

	g_free(member->name);
	g_free(member->description);
	rpct_typei_free(member->type);
	g_hash_table_unref(member->constraints);
	g_free(member);
}

static void
rpct_constraint_free(struct rpct_constraint *constraint)
{

	/* XXX  Implement constraints*/
	g_free(constraint);
}

static inline bool
rpct_type_is_fully_specialized(struct rpct_typei *inst)
{
	struct rpct_typei *s;

	if (!inst->type->generic)
		return (true);

	return (g_hash_table_size(inst->specializations)
	    == inst->type->generic_vars->len);
}

static inline struct rpct_typei *
rpct_unwind_typei(struct rpct_typei *typei)
{
	struct rpct_typei *current = typei;

	while (current) {
		if (current->type->clazz == RPC_TYPING_TYPEDEF) {
			current->type->definition;
			continue;
		}

		return (current);
	}

	return (NULL);
}

static inline bool
rpct_type_is_compatible(struct rpct_typei *decl, struct rpct_typei *type)
{
	struct rpct_type *parent_type;
	bool compatible = false;

	if (g_strcmp0(decl->type->realm, type->type->realm) != 0)
		return (false);

	if (g_hash_table_size(decl->specializations) < g_hash_table_size(type->specializations))
		return (false);

	if (g_strcmp0(decl->type->name, type->type->name) != 0) {
		parent_type = type->type;
		while (1) {
			parent_type = parent_type->parent;
			if (parent_type == NULL)
				break;

			if (g_strcmp0(parent_type->name,
			    type->type->name) == 0) {
				compatible = true;
				break;
			}
		}
	} else
		compatible = true;

	if (!compatible)
		return (false);

	/*for (guint i = 0; i < type->specializations->len; i++) {
		compatible = rpct_type_is_compatible(
		    g_ptr_array_index(decl->specializations, i),
		    g_ptr_array_index(type->specializations, i));

		if (!compatible)
			break;
	}*/

	return (compatible);
}

static int
rpct_parse_type(const char *decl, GPtrArray *variables)
{
	int nesting = 0;
	int groups = 0;
	size_t len = strlen(decl);
	size_t i;
	size_t istart = 0;

	for (i = 0; i < len; i++) {
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

				istart = i + 1;
			}
			break;

		default:
			continue;
		}
	}

	groups++;
	g_ptr_array_add(variables, g_strndup(&decl[istart], (gsize)(i - istart)));
	return (groups);
}

static char *
rpct_canonical_type(struct rpct_typei *typei)
{
	GString *ret;
	GHashTableIter iter;
	struct rpct_typei *value;
	const char *key;
	char *substr;
	guint i = 0;

	if (typei->proxy)
		return (g_strdup(typei->variable));

	ret = g_string_new(typei->type->name);

	if (!typei->type->generic)
		return (g_string_free(ret, false));

	g_string_append(ret, "<");
	g_hash_table_iter_init(&iter, typei->specializations);

	while (g_hash_table_iter_next(&iter, (gpointer *)&key, (gpointer *)&value)) {
		substr = rpct_canonical_type(value);
		g_string_append(ret, substr);
		g_free(substr);

		if (i < g_hash_table_size(typei->specializations)- 1)
			g_string_append(ret, ",");

		i++;
	}

	g_string_append(ret, ">");
	return (g_string_free(ret, false));
}

static int
rpct_lookup_type(const char *realm_name, const char *name, const char **decl,
    rpc_object_t *result)
{
	GHashTableIter iter;
	GRegex *regex;
	const char *filename;
	struct rpct_file *file;
	__block int ret = -1;

	g_hash_table_iter_init(&iter, context->files);
	regex = g_regex_new(TYPE_REGEX, 0, G_REGEX_MATCH_NOTEMPTY, NULL);

	while (g_hash_table_iter_next(&iter, (gpointer *)&filename,
	    (gpointer *)&file)) {
		if (g_strcmp0(file->realm, realm_name) != 0)
			continue;

		debugf("looking for %s::%s in %s", realm_name, name, filename);

		rpc_dictionary_apply(file->body,
		    ^(const char *key, rpc_object_t value) {
			GMatchInfo *m;

			if (!g_regex_match(regex, key, 0, &m))
				return ((bool)true);

			if (g_strcmp0(g_match_info_fetch(m, 2), name) == 0) {
				*decl = key;
				*result = value;
				ret = 0;
				g_match_info_free(m);
				return ((bool)false);
			}

			g_match_info_free(m);
			return ((bool)true);
		});
	}

	g_regex_unref(regex);
	return (ret);
}

static int
rpct_read_type(const char *realm, const char *decl, rpc_object_t obj)
{
	struct rpct_type *type;
	struct rpct_type *parent = NULL;
	struct rpct_class_handler *handler;
	const char *inherits = NULL;
	const char *description = "";
	const char *decltype, *declname, *declvars, *type_def = NULL;
	GError *err = NULL;
	GRegex *regex;
	GMatchInfo *match;
	rpc_object_t members = NULL;
	rpct_realm_t realm_obj;

	debugf("reading type \"%s\" from realm \"%s\"", decl, realm);

	realm_obj = rpct_find_realm(realm);
	rpc_object_unpack(obj, "{s,s,s,v}",
	    "inherits", &inherits,
	    "description", &description,
	    "type", &type_def,
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
	declvars = g_match_info_fetch(match, 4);

	/* If type already exists in given realm, do nothing */
	if (g_hash_table_contains(realm_obj->types, declname))
		return (0);

	type = g_malloc0(sizeof(*type));
	type->name = g_strdup(declname);
	type->realm = g_strdup(realm);
	type->parent = parent;
	type->members = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
	    (GDestroyNotify)rpct_member_free);
	type->constraints = g_hash_table_new_full(g_str_hash, g_str_equal,
	    g_free, (GDestroyNotify)rpct_constraint_free);
	type->description = g_strdup(description);
	type->generic_vars = g_ptr_array_new_with_free_func(g_free);

	for (handler = &class_handlers[0]; handler->name != NULL; handler++) {
		if (g_strcmp0(handler->name, decltype) == 0) {
			type->clazz = handler->id;
			break;
		}
	}

	if (handler == NULL) {
		/* XXX error */
	}

	if (declvars) {
		type->generic = true;
		rpct_parse_type(declvars, type->generic_vars);
	}

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
		if (rpc_dictionary_apply(members, ^(const char *key,
		    rpc_object_t value) {
			struct rpct_member *m;

			m = handler->member_fn(key, value, type);
			if (m == NULL)
				return ((bool)false);

			g_hash_table_insert(type->members,
			    g_strdup(key), m);
			return ((bool)true);
		})) {
			rpct_type_free(type);
			return (-1);
		}
	}

	if (type_def != NULL) {
		type->definition = rpct_instantiate_type(type_def, realm, NULL,
		    type);

		g_assert_nonnull(type->definition);
	}

	/* XXX Add constraints support */

	g_hash_table_insert(realm_obj->types, g_strdup(declname), type);
	debugf("inserted type %s::%s", realm, declname);
	return (0);
}

static int
rpct_read_func(const char *realm, const char *decl, rpc_object_t obj)
{
	int ret = -1;
	struct rpct_function *func = NULL;
	GError *err = NULL;
	GRegex *regex = NULL;
	GMatchInfo *match = NULL;
	const char *name;
	const char *description = "";
	const char *returns_type;
	rpc_object_t decl_obj;
	rpc_object_t args;
	rpc_object_t returns;
	rpct_realm_t realm_obj;

	decl_obj = rpc_dictionary_get_value(obj, decl);

	rpc_object_unpack(decl_obj, "{s,s,v}",
	    "description", &description,
	    "arguments", &args,
	    "return", &returns);

	regex = g_regex_new(FUNC_REGEX, 0, G_REGEX_MATCH_NOTEMPTY, &err);
	if (err != NULL)
		goto error;

	if (!g_regex_match(regex, decl, 0, &match))
		goto error;

	if (g_match_info_get_match_count(match) < 1)
		goto error;

	name = g_match_info_fetch(match, 1);
	func = g_malloc0(sizeof(*func));
	func->arguments = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
	    (GDestroyNotify)rpct_typei_free);

	if (args != NULL) {
		if (rpc_array_apply(args, ^(size_t idx __unused,
		    rpc_object_t i) {
			const char *arg_name;
			const char* arg_type;
			struct rpct_typei *arg_inst;

			arg_name = rpc_dictionary_get_string(i, "name");
			if (arg_name == NULL)
				return ((bool)false);

			arg_type = rpc_dictionary_get_string(i, "type");
			if (arg_type == NULL)
				return ((bool)false);

			arg_inst = rpct_instantiate_type(arg_type, realm,
			    NULL, NULL);
			if (arg_inst == NULL)
				return ((bool)false);

			g_hash_table_insert(func->arguments, g_strdup(arg_name),
			    arg_inst);

			return ((bool)true);
		}))
			goto error;
	}

	if (returns != NULL) {
		returns_type = rpc_string_get_string_ptr(returns);
		func->result = rpct_instantiate_type(returns_type, realm,
		     NULL, NULL);
		if (func->result == NULL)
			goto error;
	}

	func->name = g_strdup(name);
	func->realm = g_strdup(realm);
	func->description = g_strdup(description);

	realm_obj = rpct_find_realm(realm);
	g_hash_table_insert(realm_obj->functions, g_strdup(name), func);
	ret = 0;
	goto done;

error:
	if (func != NULL) {
		g_hash_table_unref(func->arguments);
		g_free(func);
	}

done:
	if (err != NULL)
		g_error_free(err);

	if (regex != NULL)
		g_regex_unref(regex);

	if (match != NULL)
		g_match_info_free(match);

	return (ret);
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
	int ret = 0;

	debugf("trying to read %s", path);

	if (!g_file_get_contents(path, &contents, &length, &err))
		return (-1);

	obj = rpc_serializer_load("yaml", contents, length);
	g_free(contents);

	if (obj == NULL)
		return (-1);

	file = g_malloc0(sizeof(*file));
	file->body = obj;
	file->path = g_strdup(path);
	file->types = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
	    NULL);

	if (rpct_read_meta(file, rpc_dictionary_get_value(obj, "meta")) < 0)
		goto error;

	g_hash_table_insert(context->files, g_strdup(path), file);

	if (rpct_find_realm(file->realm) == NULL) {
		realm = g_malloc0(sizeof(*realm));
		realm->name = g_strdup(file->realm);
		realm->types = g_hash_table_new_full(g_str_hash, g_str_equal,
		    g_free, (GDestroyNotify)rpct_type_free);
		realm->functions= g_hash_table_new_full(g_str_hash, g_str_equal,
		    g_free, (GDestroyNotify)rpct_function_free);
		g_hash_table_insert(context->realms, g_strdup(realm->name),
		    realm);
	}

	if (rpc_dictionary_apply(obj, ^(const char *key,
	    rpc_object_t v) {
		if (g_strcmp0(key, "meta") == 0)
			return ((bool)true);

		if (g_str_has_prefix(key, "function")) {
			if (rpct_read_func(file->realm, key, v) != 0)
				return ((bool)false);
			return ((bool)true);
		}

	    	rpct_read_type(file->realm, key, v);
		return ((bool)true);
	}))

	ret = -1;
	rpc_release(obj);
	return (ret);

error:
	g_hash_table_destroy(file->types);
	g_free(file->path);
	g_free(file->description);
	g_free(file->realm);
	g_free(file);
	return (-1);
}

static bool
rpct_run_validators(struct rpct_typei *typei, rpc_object_t obj,
    struct rpct_error_context *errctx)
{
	GHashTableIter iter;
	const struct rpct_validator *v;
	struct rpct_validation_result *result;
	const char *typename = rpc_get_type_name(rpc_get_type(obj));
	const char *key;
	rpc_object_t value;
	bool valid = true;

	/* Run validators */
	g_hash_table_iter_init(&iter, typei->constraints);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		v = rpc_find_validator(typename, key);
		if (v == NULL) {
			rpct_add_error(errctx, "Validator %s not found", key);
			valid = false;
			continue;
		}

		result = v->validate(obj, value, typei);
		if (result == NULL) {
			/* NULL means OK */
			continue;
		}

		if (result->valid) {
			/* Also OK */
			continue;
		}

		valid = false;
		rpct_add_error(errctx, "%s", result->error, result->extra);
	}

	return (valid);
}

static bool
rpct_validate_struct(struct rpct_typei *typei, rpc_object_t obj,
    struct rpct_error_context *errctx)
{
	__block bool valid;

	rpct_members_apply(typei->type, ^(struct rpct_member *member) {
		struct rpct_error_context newctx;
		struct rpct_typei *mtypei;
		rpc_object_t mvalue;

		mtypei = rpct_instantiate_member(member, typei);
		mvalue = rpc_dictionary_get_value(obj, member->name);

		rpct_derive_error_context(&newctx, errctx, member->name);
		rpct_validate_instance(mtypei, mvalue, &newctx);
	    	rpct_release_error_context(&newctx);

	    	return ((bool)true);
	});

	return (rpct_run_validators(typei, obj, errctx));
}

static bool
rpct_validate_builtin(struct rpct_typei *typei, rpc_object_t obj,
    struct rpct_error_context *errctx)
{

	switch (rpc_get_type(obj)) {
	case RPC_TYPE_ARRAY:
		break;

	case RPC_TYPE_DICTIONARY:
		break;
	}

	return (rpct_run_validators(typei, obj, errctx));
}

static bool
rpct_validate_instance(struct rpct_typei *typei, rpc_object_t obj,
    struct rpct_error_context *errctx)
{
	struct rpct_error_context newctx;
	struct rpct_class_handler *handler;
	bool valid;

	/* Step 1: check type */
	if (!rpct_type_is_compatible(typei, obj->ro_typei)) {
		rpct_add_error(errctx,
		    "Incompatible type %s, should be %s",
		    obj->ro_typei->canonical_form,
		    typei->canonical_form, NULL);

		valid = false;
		goto done;
	}

	/* Step 2: find class handler */
	for (handler = &class_handlers[0]; handler->name != NULL; handler++) {
		if (handler->id == typei->type->clazz)
			break;
	}

	/* Step 3: run per-class validator */
	valid = handler->validate_fn(typei, obj, errctx);

done:
	return (valid);
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
	rpct_type_t type;
	rpct_realm_t realm;
	const char **b;

	context = g_malloc0(sizeof(*context));
	context->files = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
	    (GDestroyNotify)rpct_file_free);
	context->realms = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
	    (GDestroyNotify)rpct_realm_free);

	realm = g_malloc(sizeof(*realm));
	realm->name = g_strdup("*");
	context->global_realm = realm->name;
	realm->types = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
	    (GDestroyNotify)rpct_type_free);
	realm->functions = g_hash_table_new_full(g_str_hash, g_str_equal,
	    g_free, (GDestroyNotify)rpct_function_free);

	for (b = builtin_types; *b != NULL; b++) {
		type = g_malloc0(sizeof(*type));
		type->name = g_strdup(*b);
		type->clazz = RPC_TYPING_BUILTIN;
		type->realm = g_strdup(realm->name);
		type->members = g_hash_table_new_full(g_str_hash, g_str_equal,
		    g_free, (GDestroyNotify)rpct_member_free);
		type->constraints = g_hash_table_new_full(g_str_hash,
		    g_str_equal, g_free, (GDestroyNotify)rpct_constraint_free);
		type->description = g_strdup_printf("builtin %s type", *b);
		type->generic_vars = g_ptr_array_new();
		g_hash_table_insert(realm->types, g_strdup(type->name), type);
	}

	g_hash_table_insert(context->realms, g_strdup(realm->name), realm);
	return (0);
}

void
rpct_free(void)
{

	g_hash_table_unref(context->files);
	g_hash_table_unref(context->realms);
	g_free(context);
}

void
rpct_typei_free(struct rpct_typei *inst)
{

	g_hash_table_destroy(inst->specializations);
	g_free(inst->canonical_form);
	g_free(inst);
}

int
rpct_load_types(const char *path)
{

	return (rpct_read_file(path));
}

int
rpct_load_types_stream(int fd)
{

}

const char *
rpct_get_realm(void)
{

	return (context->global_realm);
}

int
rpct_set_realm(const char *realm)
{
	struct rpct_realm *realm_obj;

	realm_obj = rpct_find_realm(realm);
	if (realm_obj == NULL)
		return (-1);

	context->global_realm = realm_obj->name;
	return (0);
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

rpct_typei_t
rpct_type_get_definition(rpct_type_t type)
{

	return (type->definition);
}

rpct_class_t
rpct_type_get_class(rpct_type_t type)
{

	return (type->clazz);
}

int
rpct_type_get_generic_vars_count(rpct_type_t type)
{
	if (!type->generic)
		return (0);

	return (type->generic_vars->len);
}

rpct_type_t
rpct_typei_get_type(rpct_typei_t typei)
{

	return (typei->type);
}

rpct_typei_t
rpct_typei_get_generic_var(rpct_typei_t typei, const char *name)
{

	return (g_hash_table_lookup(typei->specializations, name));
}

const char *
rpct_typei_get_canonical_form(rpct_typei_t typei)
{

	return (typei->canonical_form);
}

rpct_member_t
rpct_type_get_member(rpct_type_t type, const char *name)
{

	return (g_hash_table_lookup(type->members, name));
}

rpct_typei_t
rpct_typei_get_member_type(rpct_typei_t typei, rpct_member_t member)
{

	return (rpct_instantiate_member(member, typei));
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

rpct_typei_t
rpct_member_get_typei(rpct_member_t member)
{

	return (member->type);
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

rpc_object_t
rpct_serialize(rpc_object_t object)
{
	rpc_object_t result;


	if (object->ro_typei == NULL)
		return (object);

	result = rpc_object_pack("{s,s,v}",
	    RPCT_REALM_FIELD, rpct_type_get_realm(object->ro_typei->type),
	    RPCT_TYPE_FIELD, object->ro_typei->canonical_form,
	    RPCT_VALUE_FIELD, object);

	rpc_retain(object);

	return (result);
}

rpc_object_t
rpct_deserialize(rpc_object_t object)
{
	rpc_object_t result;

	if (rpc_get_type(object) != RPC_TYPE_DICTIONARY)
		return (NULL);

	if (!rpc_dictionary_has_key(object, RPCT_REALM_FIELD))
		return (NULL);

	if (!rpc_dictionary_has_key(object, RPCT_TYPE_FIELD))
		return (NULL);

	if (!rpc_dictionary_has_key(object, RPCT_VALUE_FIELD))
		return (NULL);

	result = rpc_dictionary_get_value(object, RPCT_VALUE_FIELD);

	result = rpct_new(rpc_dictionary_get_string(object,RPCT_TYPE_FIELD),
	    rpc_dictionary_get_string(object, RPCT_REALM_FIELD), result);

	rpc_retain(result);

	return (result);
}

struct rpct_validation_result *
rpct_validation_result_new(bool valid, const char *format, ...)
{
	struct rpct_validation_result *result;
	va_list ap;

	va_start(ap, format);
	result = g_malloc0(sizeof(*result));
	result->valid = valid;
	result->error = g_strdup_vprintf(format, ap);
	result->extra = va_arg(ap, rpc_object_t);
	va_end(ap);

	if (result->extra != NULL)
		rpc_retain(result->extra);

	return (result);
}

void
rpct_validation_result_free(struct rpct_validation_result *result)
{
	if (result->error != NULL)
		g_free(result->error);

	if (result->extra != NULL)
		rpc_release(result->extra);

	g_free(result);
}

static void
rpct_derive_error_context(struct rpct_error_context *newctx,
    struct rpct_error_context *oldctx, const char *name)
{

	newctx->path = g_strdup_printf("%s.%s", oldctx->path, name);
	newctx->errors = oldctx->errors;
}

static void
rpct_release_error_context(struct rpct_error_context *ctx)
{

	g_free(ctx->path);
}

static void
rpct_add_error(struct rpct_error_context *ctx, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	va_end(ap);
}