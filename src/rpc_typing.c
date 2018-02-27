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

#include <errno.h>
#include <glib.h>
#include <yaml.h>
#include <rpc/object.h>
#include <rpc/serializer.h>
#include "internal.h"

static int rpct_read_meta(struct rpct_file *file, rpc_object_t obj);
static int rpct_lookup_type(const char *name, const char **decl,
    rpc_object_t *result, struct rpct_file **filep);
static struct rpct_type *rpct_find_type(const char *name);
static struct rpct_type *rpct_find_type_fuzzy(const char *name,
    struct rpct_file *origin);
static inline bool rpct_type_is_fully_specialized(struct rpct_typei *inst);
static inline struct rpct_typei *rpct_unwind_typei(struct rpct_typei *typei);
static char *rpct_canonical_type(struct rpct_typei *typei);
static int rpct_read_type(struct rpct_file *file, const char *decl,
    rpc_object_t obj);
static bool rpct_validate_args(struct rpct_if_member *func, rpc_object_t args);
static bool rpct_validate_return(struct rpct_if_member *func,
    rpc_object_t result);
static int rpct_parse_type(const char *decl, GPtrArray *variables);

static struct rpct_context *context = NULL;
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

	return (rpct_instantiate_type(decl, NULL, NULL, NULL));
}

rpc_object_t
rpct_new(const char *decl, rpc_object_t object)
{
	struct rpct_typei *typei;

	typei = rpct_instantiate_type(decl, NULL, NULL, NULL);
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

	return rpct_find_type(name);
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

static struct rpct_type *
rpct_find_type_fuzzy(const char *name, struct rpct_file *origin)
{
	struct rpct_type *result;
	char *full_name;
	const char *prefix;
	guint i;

	result = rpct_find_type(name);
	if (result != NULL)
		return (result);

	if (origin == NULL)
		return (NULL);

	if (origin->ns != NULL) {
		full_name = g_strdup_printf("%s.%s", origin->ns, name);
		result = rpct_find_type(full_name);
		g_free(full_name);
		if (result != NULL)
			return (result);
	}

	for (i = 0; i < origin->uses->len; i++) {
		prefix = g_ptr_array_index(origin->uses, i);
		full_name = g_strdup_printf("%s.%s", prefix, name);
		result = rpct_find_type(full_name);
		g_free(full_name);
		if (result != NULL)
			return (result);
	}

	return (NULL);
}

static struct rpct_type *
rpct_find_type(const char *name)
{
	struct rpct_file *file;
	rpct_type_t type = NULL;

	type = g_hash_table_lookup(context->types, name);

	if (type == NULL) {
		const char *decl;
		rpc_object_t obj;

		debugf("type %s not found, trying to look it up", name);

		if (rpct_lookup_type(name, &decl, &obj, &file) == 0)
			rpct_read_type(file, decl, obj);

		debugf("hopefully %s is loaded now", name);

		type = g_hash_table_lookup(context->types, name);
		if (type != NULL)
			debugf("successfully chain-loaded %s", name);
	}

	return (type);

}

static int
rpct_read_meta(struct rpct_file *file, rpc_object_t obj)
{
	rpc_object_t uses = NULL;
	int ret;

	if (obj == NULL) {
		rpc_set_last_error(EINVAL, "meta section corrupted", NULL);
		return (-1);
	}

	ret = rpc_object_unpack(obj, "{s,s,i,v}",
	    "version", &file->version,
	    "namespace", &file->ns,
	    "description", &file->description,
	    "use", &uses);

	if (uses != NULL) {
		rpc_array_apply(uses, ^(size_t idx, rpc_object_t value) {
			g_ptr_array_add(file->uses, g_strdup(
			    rpc_string_get_string_ptr(value)));
			return ((bool)true);
		});
	}

	return (ret > 0 ? 0 : -1);
}

struct rpct_typei *
rpct_instantiate_type(const char *decl, struct rpct_typei *parent,
    struct rpct_type *ptype, struct rpct_file *origin)
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

	debugf("instantiating type %s", decl);

	regex = g_regex_new(INSTANCE_REGEX, 0, G_REGEX_MATCH_NOTEMPTY, &err);
	g_assert_no_error(err);

	if (!g_regex_match(regex, decl, 0, &match)) {
		rpc_set_last_errorf(EINVAL, "Invalid type specification: %s",
		    decl);
		goto error;
	}

	if (g_match_info_get_match_count(match) < 1) {
		rpc_set_last_errorf(EINVAL, "Invalid type specification: %s",
		    decl);
		goto error;
	}

	decltype = g_match_info_fetch(match, 1);
	type = rpct_find_type_fuzzy(decltype, origin);
	if (type == NULL) {
		struct rpct_typei *cur = parent;

		debugf("type %s not found, maybe it's a generic variable",
		    decltype);

		while (cur != NULL) {
			/* Maybe it's a type variable? */
			if (cur->type->generic) {
				subtype = g_hash_table_lookup(
				    cur->specializations, decltype);

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

		rpc_set_last_errorf(EINVAL, "Type %s not found", decl);
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
		if (declvars == NULL) {
			rpc_set_last_errorf(EINVAL,
			    "Invalid generic variable specification: %s",
			    decl);
			goto error;
		}

		splitvars = g_ptr_array_new();
		rpct_parse_type(declvars, splitvars);

		if (splitvars->len != type->generic_vars->len)
			goto error;

		for (guint i = 0; i < splitvars->len; i++) {
			const char *var = g_ptr_array_index(type->generic_vars, i);
			const char *vartype = g_ptr_array_index(splitvars, i);

			subtype = rpct_instantiate_type(vartype, ret, ptype, origin);
			if (subtype == NULL) {
				rpc_set_last_errorf(EINVAL,
				    "Cannot instantiate generic type %s in %s",
				    vartype, decltype);
				goto error;
			}

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
	    parent, parent->type, parent->type->file);
	ret->constraints = member->constraints;
	return (ret);
}

static void
rpct_type_free(rpct_type_t type)
{

	g_free(type->name);
	g_free(type->description);
	g_ptr_array_free(type->generic_vars, true);
	g_hash_table_destroy(type->members);
	g_hash_table_destroy(type->constraints);
	g_free(type);
}

static void
rpct_file_free(struct rpct_file *file)
{

	g_free(file->path);
	g_hash_table_destroy(file->types);
	g_free(file);
}

static void
rpct_if_member_free(struct rpct_if_member *member)
{

	g_free(member->description);
	g_ptr_array_free(member->arguments, true);
	if (member->result != NULL)
		rpct_typei_free(member->result);

	g_free(member);
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

static inline bool
rpct_type_is_fully_specialized(struct rpct_typei *inst)
{

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
			current = current->type->definition;
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

	if (g_strcmp0(decl->type->name, "any") == 0)
		return (true);

	if (g_hash_table_size(decl->specializations) <
	    g_hash_table_size(type->specializations))
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
rpct_lookup_type(const char *name, const char **decl, rpc_object_t *result,
    struct rpct_file **filep)
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

		debugf("looking for %s in %s", name, filename);

		rpc_dictionary_apply(file->body,
		    ^(const char *key, rpc_object_t value) {
			GMatchInfo *m;
			char *full_name;

			if (!g_regex_match(regex, key, 0, &m))
				return ((bool)true);

			full_name = file->ns != NULL
			    ? g_strdup_printf("%s.%s", file->ns, g_match_info_fetch(m, 2))
			    : g_strdup(g_match_info_fetch(m, 2));

			if (g_strcmp0(full_name, name) == 0) {
				*decl = key;
				*result = value;
				*filep = file;
				ret = 0;
				g_match_info_free(m);
				g_free(full_name);
				return ((bool)false);
			}

			g_match_info_free(m);
			g_free(full_name);
			return ((bool)true);
		});
	}

	g_regex_unref(regex);
	return (ret);
}

static int
rpct_read_type(struct rpct_file *file, const char *decl, rpc_object_t obj)
{
	struct rpct_type *type;
	struct rpct_type *parent = NULL;
	const struct rpct_class_handler *handler;
	char *typename;
	const char *inherits = NULL;
	const char *description = "";
	const char *decltype, *declname, *declvars, *type_def = NULL;
	GError *err = NULL;
	GRegex *regex;
	GMatchInfo *match;
	rpc_object_t members = NULL;

	debugf("reading type \"%s\"", decl);

	rpc_object_unpack(obj, "{s,s,s,v}",
	    "inherits", &inherits,
	    "description", &description,
	    "type", &type_def,
	    "members", &members);

	if (inherits != NULL) {
		parent = rpct_find_type_fuzzy(inherits, file);
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

	typename = file->ns != NULL
	    ? g_strdup_printf("%s.%s", file->ns, declname)
	    : g_strdup(declname);

	/* If type already exists, do nothing */
	if (g_hash_table_contains(context->types, typename))
		return (0);

	type = g_malloc0(sizeof(*type));
	type->name = typename;
	type->file = file;
	type->parent = parent;
	type->members = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
	    (GDestroyNotify)rpct_member_free);
	type->constraints = g_hash_table_new_full(g_str_hash, g_str_equal,
	    g_free, (GDestroyNotify)rpc_release_impl);
	type->description = g_strdup(description);
	type->generic_vars = g_ptr_array_new_with_free_func(g_free);

	handler = rpc_find_class_handler(decltype, (rpct_class_t)-1);
	if (handler == NULL) {
		g_regex_unref(regex);
		g_match_info_free(match);
		rpct_type_free(type);
		return (-1);
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
		type->clazz = RPC_TYPING_TYPEDEF;
		type->definition = rpct_instantiate_type(type_def, NULL, type, file);

		g_assert_nonnull(type->definition);
	}

	if (!g_hash_table_insert(context->types, g_strdup(type->name), type))
		g_assert_not_reached();

	debugf("inserted type %s", declname);
	return (0);
}

static int
rpct_read_property(struct rpct_file *file, struct rpct_interface *iface,
    const char *decl, rpc_object_t obj)
{
	struct rpct_if_member *prop;
	GError *err = NULL;
	GRegex *regex = NULL;
	GMatchInfo *match = NULL;
	const char *name;
	const char *description = NULL;
	const char *type = NULL;
	bool read_only = false;
	bool read_write = false;
	bool write_only = false;
	bool notify = false;

	rpc_object_unpack(obj, "{s,s,b,b,b,b}",
	    "description", &description,
	    "type", &type,
	    "read-only", &read_only,
	    "read-write", &read_write,
	    "write-only", &write_only,
	    "notify", &notify);

	regex = g_regex_new(PROPERTY_REGEX, 0, G_REGEX_MATCH_NOTEMPTY, &err);
	g_assert_no_error(err);

	if (!g_regex_match(regex, decl, 0, &match)) {
		rpc_set_last_errorf(EINVAL, "Cannot parse: %s", decl);
		goto error;
	}

	if (g_match_info_get_match_count(match) < 1) {
		rpc_set_last_errorf(EINVAL, "Cannot parse: %s", decl);
		goto error;
	}

	name = g_match_info_fetch(match, 1);
	prop = g_malloc0(sizeof(*prop));
	prop->member.rim_name = name;
	prop->member.rim_type = RPC_MEMBER_PROPERTY;
	prop->description = g_strdup(description);

	if (!read_only && !write_only && !read_write) {
		rpc_set_last_errorf(EINVAL, "Property %s has no access "
		    "rights defined", name);
		goto error;
	}

	if (type)
		prop->result = rpct_instantiate_type(type, NULL, NULL, file);

	g_hash_table_insert(iface->members, g_strdup(name), prop);
	return (0);

error:
	if (match != NULL)
		g_match_info_free(match);

	g_regex_unref(regex);
	return (-1);
}

static int
rpct_read_event(struct rpct_file *file, struct rpct_interface *iface,
    const char *decl, rpc_object_t obj)
{
	struct rpct_if_member *prop;
	GError *err = NULL;
	GRegex *regex = NULL;
	GMatchInfo *match = NULL;
	const char *name;
	const char *description = NULL;
	const char *type = NULL;

	rpc_object_unpack(obj, "{s,s}",
	    "description", &description,
	    "type", &type);

	regex = g_regex_new(EVENT_REGEX, 0, G_REGEX_MATCH_NOTEMPTY, &err);
	g_assert_no_error(err);

	if (!g_regex_match(regex, decl, 0, &match)) {
		rpc_set_last_errorf(EINVAL, "Cannot parse: %s", decl);
		goto error;
	}

	if (g_match_info_get_match_count(match) < 1) {
		rpc_set_last_errorf(EINVAL, "Cannot parse: %s", decl);
		goto error;
	}

	name = g_match_info_fetch(match, 1);
	prop = g_malloc0(sizeof(*prop));
	prop->member.rim_name = name;
	prop->member.rim_type = RPC_MEMBER_EVENT;
	prop->description = g_strdup(description);

	if (type)
		prop->result = rpct_instantiate_type(type, NULL, NULL, file);

	g_hash_table_insert(iface->members, g_strdup(name), prop);
	return (0);

error:
	if (match != NULL)
		g_match_info_free(match);

	g_regex_unref(regex);
	return (-1);
}

static int
rpct_read_method(struct rpct_file *file, struct rpct_interface *iface,
    const char *decl, rpc_object_t obj)
{
	int ret = -1;
	struct rpct_if_member *method = NULL;
	GError *err = NULL;
	GRegex *regex = NULL;
	GMatchInfo *match = NULL;
	const char *name;
	const char *description = "";
	const char *returns_type;
	rpc_object_t args = NULL;
	rpc_object_t returns = NULL;

	debugf("reading <%s> from file %s", decl, file->path);

	rpc_object_unpack(obj, "{s,v,v}",
	    "description", &description,
	    "args", &args,
	    "return", &returns);

	regex = g_regex_new(METHOD_REGEX, 0, G_REGEX_MATCH_NOTEMPTY, &err);
	g_assert_no_error(err);

	if (!g_regex_match(regex, decl, 0, &match)) {
		rpc_set_last_errorf(EINVAL, "Cannot parse: %s", decl);
		goto error;
	}

	if (g_match_info_get_match_count(match) < 1) {
		rpc_set_last_errorf(EINVAL, "Cannot parse: %s", decl);
		goto error;
	}

	name = g_match_info_fetch(match, 1);
	method = g_malloc0(sizeof(*method));
	method->member.rim_name = name;
	method->member.rim_type = RPC_MEMBER_METHOD;
	method->arguments = g_ptr_array_new();

	if (args != NULL) {
		if (rpc_array_apply(args, ^(size_t idx __unused,
		    rpc_object_t i) {
			const char *arg_name;
			const char* arg_type;
			struct rpct_argument *arg;
			struct rpct_typei *arg_inst;

			arg_name = rpc_dictionary_get_string(i, "name");
			if (arg_name == NULL) {
				rpc_set_last_errorf(EINVAL,
				    "Required 'name' field in argument %d "
				    "of %s missing", idx, name);
				return ((bool)false);
			}

			arg_type = rpc_dictionary_get_string(i, "type");
			if (arg_type == NULL) {
				rpc_set_last_errorf(EINVAL,
				    "Required 'type' field in argument %d "
				    "of %s missing", idx, name);
				return ((bool)false);
			}

			arg_inst = rpct_instantiate_type(arg_type, NULL, NULL, file);
			if (arg_inst == NULL)
				return ((bool)false);

			arg = g_malloc0(sizeof(*arg));
			arg->name = g_strdup(arg_name);
			arg->description = g_strdup(rpc_dictionary_get_string(i, "description"));
			arg->type = arg_inst;
			g_ptr_array_add(method->arguments, arg);
			return ((bool)true);
		}))
			goto error;
	}

	if (returns != NULL) {
		returns_type = rpc_dictionary_get_string(returns, "type");
		method->result = rpct_instantiate_type(returns_type, NULL, NULL, file);
		if (method->result == NULL) {
			rpc_set_last_errorf(EINVAL,
			    "Cannot instantiate return type %s of method %s",
			    returns_type, name);
			goto error;
		}
	}

	method->description = g_strdup(description);

	g_hash_table_insert(iface->members, g_strdup(name), method);
	ret = 0;
	goto done;

error:
	if (method != NULL) {
		g_ptr_array_free(method->arguments, true);
		g_free(method);
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
rpct_read_interface(struct rpct_file *file, const char *decl, rpc_object_t obj)
{
	struct rpct_interface *iface;
	GError *err = NULL;
	GRegex *regex = NULL;
	GMatchInfo *match = NULL;
	bool result;

	regex = g_regex_new(INTERFACE_REGEX, 0, 0, &err);
	g_assert_no_error(err);

	if (!g_regex_match(regex, decl, 0, &match))
		return (-1);

	if (g_match_info_get_match_count(match) < 1)
		return (-1);

	iface = g_malloc0(sizeof(*iface));
	iface->name = g_match_info_fetch(match, 1);
	iface->members = g_hash_table_new(g_str_hash, g_str_equal);
	iface->description = g_strdup(rpc_dictionary_get_string(obj,
	    "description"));

	if (file->ns)
		iface->name = g_strdup_printf("%s.%s", file->ns, iface->name);

	result = rpc_dictionary_apply(obj, ^(const char *key, rpc_object_t v) {
		if (g_str_has_prefix(key, "property")) {
			if (rpct_read_property(file, iface, key, v) != 0)
				return ((bool)false);
		}

		if (g_str_has_prefix(key, "method")) {
			if (rpct_read_method(file, iface, key, v) != 0)
				return ((bool)false);
		}

		if (g_str_has_prefix(key, "event")) {
			if (rpct_read_event(file, iface, key, v) != 0)
				return ((bool)false);
		}

		return ((bool)true);
	});

	if (result)
		return (-1);

	g_hash_table_insert(context->interfaces, iface->name, iface);
	g_hash_table_insert(file->interfaces, iface->name, iface);
	return (0);
}

int
rpct_read_file(const char *path)
{
	struct rpct_file *file;
	char *contents;
	size_t length;
	rpc_object_t obj;
	GError *err = NULL;

	debugf("trying to read %s", path);

	if (g_hash_table_contains(context->files, path)) {
		debugf("file %s already loaded", path);
		return (0);
	}

	if (!g_file_get_contents(path, &contents, &length, &err)) {
		rpc_set_last_gerror(err);
		return (-1);
	}

	obj = rpc_serializer_load("yaml", contents, length);
	g_free(contents);

	if (obj == NULL)
		return (-1);

	file = g_malloc0(sizeof(*file));
	file->body = rpc_retain(obj);
	file->path = g_strdup(path);
	file->uses = g_ptr_array_new_with_free_func(g_free);
	file->types = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
	    NULL);
	file->interfaces = g_hash_table_new(g_str_hash, g_str_equal);

	if (rpct_read_meta(file, rpc_dictionary_get_value(obj, "meta")) < 0) {
		rpc_set_last_errorf(EINVAL,
		    "Cannot read meta section of file %s", file->path);
		return (-1);
	}

	g_hash_table_insert(context->files, g_strdup(path), file);
	return (0);
}

bool
rpct_run_validators(struct rpct_typei *typei, rpc_object_t obj,
    struct rpct_error_context *errctx)
{
	GHashTableIter iter;
	const struct rpct_validator *v;
	const char *typename = rpc_get_type_name(rpc_get_type(obj));
	const char *key;
	rpc_object_t value;
	bool valid = true;

	/* Run validators */
	g_hash_table_iter_init(&iter, typei->constraints);
	while (g_hash_table_iter_next(&iter, (gpointer)&key, (gpointer)&value)) {
		v = rpc_find_validator(typename, key);
		if (v == NULL) {
			rpct_add_error(errctx, "Validator %s not found", key);
			valid = false;
			continue;
		}

		debugf("Running validator %s on %s", key, typename);
		if (!v->validate(obj, value, typei, errctx))
			valid = false;
	}

	return (valid);
}

bool
rpct_validate_instance(struct rpct_typei *typei, rpc_object_t obj,
    struct rpct_error_context *errctx)
{
	const struct rpct_class_handler *handler;
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

	handler = rpc_find_class_handler(NULL, typei->type->clazz);
	g_assert_nonnull(handler);

	/* Step 3: run per-class validator */
	valid = handler->validate_fn(typei, obj, errctx);

done:
	return (valid);
}

static bool
rpct_validate_args(struct rpct_if_member *func, rpc_object_t args)
{
	return (rpc_array_apply(args, ^(size_t idx, rpc_object_t i) {
		rpc_object_t errors;
		rpct_typei_t typei = g_ptr_array_index(func->arguments, idx);

		return (rpct_validate(typei, i, &errors));
	}));
}

static bool
rpct_validate_return(struct rpct_if_member *func, rpc_object_t result)
{

	rpc_set_last_errorf(ENOTSUP, "Not implemented");
	return (-1);
}

bool
rpct_validate(struct rpct_typei *typei, rpc_object_t obj, rpc_object_t *errors)
{
	struct rpct_validation_error *err;
	struct rpct_error_context errctx;
	bool valid;
	guint i;

	errctx.path = "";
	errctx.errors = g_ptr_array_new();

	valid = rpct_validate_instance(typei, obj, &errctx);

	if (errors != NULL) {
		*errors = rpc_array_create();
		for (i = 0; i < errctx.errors->len; i++) {
			err = g_ptr_array_index(errctx.errors, i);
			rpc_array_append_stolen_value(*errors,
			    rpc_object_pack("{s,s,v}",
			        "path", err->path,
			        "message", err->message,
			        "extra", err->extra));
		}
	}

	return (valid);
}

int
rpct_init(void)
{
	rpct_type_t type;
	const char **b;

	context = g_malloc0(sizeof(*context));
	context->files = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
	    (GDestroyNotify)rpct_file_free);
	context->types = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
	    (GDestroyNotify)rpct_type_free);
	context->interfaces = g_hash_table_new_full(g_str_hash, g_str_equal,
	    g_free, (GDestroyNotify)rpct_if_member_free);

	for (b = builtin_types; *b != NULL; b++) {
		type = g_malloc0(sizeof(*type));
		type->name = g_strdup(*b);
		type->clazz = RPC_TYPING_BUILTIN;
		type->members = g_hash_table_new_full(g_str_hash, g_str_equal,
		    g_free, (GDestroyNotify)rpct_member_free);
		type->constraints = g_hash_table_new_full(g_str_hash,
		    g_str_equal, g_free, (GDestroyNotify)rpc_release_impl);
		type->description = g_strdup_printf("builtin %s type", *b);
		type->generic_vars = g_ptr_array_new();
		g_hash_table_insert(context->types, g_strdup(type->name), type);
	}

	return (0);
}

void
rpct_free(void)
{

	g_hash_table_unref(context->files);
	g_free(context);
}

void
rpct_typei_free(struct rpct_typei *inst)
{

	if (inst->specializations != NULL)
		g_hash_table_destroy(inst->specializations);

	g_free(inst->canonical_form);
	g_free(inst);
}

int
rpct_load_types(const char *path)
{
	struct rpct_file *file;

	if (rpct_read_file(path) != 0)
		return (-1);

	file = g_hash_table_lookup(context->files, path);
	g_assert_nonnull(file);

	if (rpc_dictionary_apply(file->body, ^(const char *key,
		rpc_object_t v) {
		if (g_strcmp0(key, "meta") == 0)
			return ((bool)true);

		if (g_str_has_prefix(key, "interface")) {
			if (rpct_read_interface(file, key, v) != 0)
				return ((bool)false);

			return ((bool)true);
	    }

		rpct_read_type(file, key, v);
		return ((bool)true);
	}))
		return (-1);

	return (0);
}

int
rpct_load_types_stream(int fd)
{

	rpc_set_last_errorf(ENOTSUP, "Not implemented");
	return (-1);
}

const char *
rpct_type_get_name(rpct_type_t type)
{

	return (type->name);
}

const char *
rpct_type_get_module(rpct_type_t type)
{

	return (type->file->path);
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

const char *
rpct_type_get_generic_var(rpct_type_t type, int index)
{
	if (index < 0 || index > (int)type->generic_vars->len)
		return (NULL);

	return (g_ptr_array_index(type->generic_vars, index));
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

const char *
rpct_interface_get_name(rpct_interface_t iface)
{

	return (iface->name);
}

const char *
rpct_interface_get_description(rpct_interface_t iface)
{

	return (iface->description);
}

enum rpc_if_member_type
rpct_if_member_get_type(rpct_if_member_t member)
{

	return (member->member.rim_type);
}

const char *
rpct_if_member_get_name(rpct_if_member_t member)
{

	return (member->member.rim_name);
}

const char *
rpct_if_member_get_description(rpct_if_member_t func)
{

	return (func->description);
}

rpct_typei_t
rpct_method_get_return_type(rpct_if_member_t method)
{

	return (method->result);
}

int
rpct_method_get_arguments_count(rpct_if_member_t method)
{

	return (method->arguments->len);
}

rpct_argument_t
rpct_method_get_argument(rpct_if_member_t method, int index)
{

	if (index < 0 || index > (int)method->arguments->len)
		return (NULL);

	return (g_ptr_array_index(method->arguments, index));
}

rpct_typei_t
rpct_property_get_type(rpct_if_member_t prop)
{

	return (prop->result);
}


const char *
rpct_argument_get_description(rpct_argument_t arg)
{

	return (arg->description);
}

rpct_typei_t
rpct_argument_get_typei(rpct_argument_t arg)
{

	return (arg->type);
}

bool
rpct_types_apply(rpct_type_applier_t applier)
{
	GHashTableIter iter;
	char *key;
	rpct_type_t value;

	g_hash_table_iter_init(&iter, context->types);
	while (g_hash_table_iter_next(&iter, (gpointer *)&key,
	    (gpointer *)&value)) {
		if (!applier(value))
			return (false);
	}

	return (true);
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

bool
rpct_interface_apply(rpct_interface_applier_t applier)
{
	GHashTableIter iter;
	char *key;
	struct rpct_interface *value;
	bool flag = false;

	g_hash_table_iter_init(&iter, context->interfaces);
	while (g_hash_table_iter_next(&iter, (gpointer *)&key,
	    (gpointer *)&value))
		if (!applier(value)) {
			flag = true;
			break;
		}

	return (flag);
}

bool
rpct_if_member_apply(rpct_interface_t iface, rpct_if_member_applier_t applier)
{
	GHashTableIter iter;
	char *key;
	struct rpct_if_member *value;
	bool flag = false;

	g_hash_table_iter_init(&iter, iface->members);
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
	const struct rpct_class_handler *handler;
	rpct_class_t clazz = object->ro_typei->type->clazz;

	if (object->ro_typei == NULL)
		return (object);

	handler = rpc_find_class_handler(NULL, clazz);
	g_assert_nonnull(handler);

	return (handler->serialize_fn(object));
}

rpc_object_t
rpct_deserialize(rpc_object_t object)
{
	const char *typename;
	rpc_type_t objtype = rpc_get_type(object);
	rpc_object_t type;
	rpc_object_t result;

	if (object->ro_typei != NULL)
		return (object);

	if (objtype == RPC_TYPE_DICTIONARY) {
		rpc_dictionary_map(object, ^(const char *key, rpc_object_t v) {
			return (rpct_deserialize(v));
		});

		type = rpc_dictionary_detach_key(object, RPCT_TYPE_FIELD);

		if (type == NULL)
			goto trivial;

		result = rpct_new(rpc_string_get_string_ptr(type),
		    object);

		if (result == NULL)
			return (rpc_null_create());

		rpc_retain(result);
		return (result);
	}

	if (objtype == RPC_TYPE_ARRAY) {
		rpc_array_map(object, ^(size_t idx, rpc_object_t v) {
			return (rpct_deserialize(v));
		});
	}

trivial:
	typename = rpc_get_type_name(rpc_get_type(object));
	return (rpct_new(typename, object));
}

void
rpct_derive_error_context(struct rpct_error_context *newctx,
    struct rpct_error_context *oldctx, const char *name)
{

	newctx->path = g_strdup_printf("%s.%s", oldctx->path, name);
	newctx->errors = oldctx->errors;
}

void
rpct_release_error_context(struct rpct_error_context *ctx)
{

	g_free(ctx->path);
}

void
rpct_add_error(struct rpct_error_context *ctx, const char *fmt, ...)
{
	va_list ap;
	struct rpct_validation_error *err;

	va_start(ap, fmt);
	err = g_malloc0(sizeof(*err));
	err->path = g_strdup(ctx->path);
	err->message = g_strdup_vprintf(fmt, ap);
	err->extra = va_arg(ap, rpc_object_t);
	va_end(ap);

	g_ptr_array_add(ctx->errors, err);
}
