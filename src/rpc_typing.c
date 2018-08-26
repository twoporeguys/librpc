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

static int rpct_read_meta(struct rpct_file *, rpc_object_t);
static int rpct_lookup_type(const char *, const char **, rpc_object_t *,
    struct rpct_file **);
static struct rpct_type *rpct_find_type(const char *);
static struct rpct_type *rpct_find_type_fuzzy(const char *, struct rpct_file *);
static rpc_object_t rpct_stream_idl(void *, rpc_object_t);
static int rpct_check_fields(rpc_object_t, ...);
#if 0
static inline bool rpct_type_is_fully_specialized(struct rpct_typei *inst);
#endif
static inline bool rpct_type_is_compatible(struct rpct_typei *,
    struct rpct_typei *);
static inline struct rpct_typei *rpct_unwind_typei(struct rpct_typei *);
static char *rpct_canonical_type(struct rpct_typei *);
static int rpct_read_type(struct rpct_file *, const char *, rpc_object_t);
static int rpct_parse_type(const char *, GPtrArray *);
static void rpct_interface_free(struct rpct_interface *);

static GRegex *rpct_instance_regex = NULL;
static GRegex *rpct_interface_regex = NULL;
static GRegex *rpct_type_regex = NULL;
static GRegex *rpct_method_regex = NULL;
static GRegex *rpct_property_regex = NULL;
static GRegex *rpct_event_regex = NULL;

static struct rpct_context *context = NULL;
static const char *builtin_types[] = {
	"nulltype",
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

static const struct rpc_if_member rpct_typing_vtable[] = {
	RPC_METHOD(download, rpct_stream_idl),
	RPC_MEMBER_END
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

	if (g_strcmp0(decl, "?") == 0)
		decl = "com.twoporeguys.librpc.Optional";

	typei = rpct_instantiate_type(decl, NULL, NULL, NULL);
	if (typei == NULL)
		return (NULL);

	return (rpct_newi(typei, object));
}

rpc_object_t
rpct_newi(rpct_typei_t typei, rpc_object_t object)
{
	if (object == NULL)
		return (NULL);

	object = rpc_copy(object);
	return (rpct_set_typei(typei, object));
}

rpc_object_t
rpct_set_typei(rpct_typei_t typei, rpc_object_t object)
{
	rpct_typei_t base_typei;
	const char *typename;

	if (object == NULL)
		return (NULL);

	typename = rpc_get_type_name(object->ro_type);
	base_typei = rpct_unwind_typei(typei);

	if (base_typei->type->clazz == RPC_TYPING_BUILTIN &&
	    g_strcmp0(base_typei->canonical_form, typename) != 0)
		return (NULL);

	//if (object->ro_typei != NULL)
	//	rpct_typei_release(object->ro_typei);

	object->ro_typei = rpct_typei_retain(typei);
	return (object);
}

rpct_class_t
rpct_get_class(rpc_object_t instance)
{

	return (instance->ro_typei->type->clazz);
}

rpct_type_t
rpct_get_type(const char *name)
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

static int
rpct_check_fields(rpc_object_t obj, ...)
{
	GPtrArray *allowed;
	const char *token;
	va_list ap;
	bool ret;

	if (rpc_get_type(obj) != RPC_TYPE_DICTIONARY) {
		rpc_set_last_errorf(EINVAL, "Declaration not a dictionary");
		return (-1);
	}

	allowed = g_ptr_array_new();
	va_start(ap, obj);

	for (;;) {
		token = va_arg(ap, const char *);
		if (token == NULL)
			break;

		g_ptr_array_add(allowed, (gpointer)token);
	}

	va_end(ap);

	ret = rpc_dictionary_apply(obj, ^(const char *key, rpc_object_t v) {
		if (!g_ptr_array_find_with_equal_func(allowed, key,
		    g_str_equal, NULL)) {
			rpc_set_last_errorf(EINVAL,
			    "Unknown field '%s' in declaration", key);
			return ((bool)false);
		}

		return ((bool)true);
	});

	g_ptr_array_free(allowed, true);
	return (ret ? -1 : 0);
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

static rpc_object_t
rpct_stream_idl(void *cookie, rpc_object_t args __unused)
{
	GHashTableIter iter;
	struct rpct_file *file;

	g_hash_table_iter_init(&iter, context->files);
	while (g_hash_table_iter_next(&iter, NULL, (gpointer)&file)) {
		rpc_function_yield(cookie, rpc_object_pack("{s,v}",
		    "name", file->path,
		    "body", rpc_retain(file->body)));
	}

	return (NULL);
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

	if (rpct_check_fields(obj, "version", "namespace", "description",
	    "use", NULL) != 0)
		return (-1);

	ret = rpc_object_unpack(obj, "{i,s,s,v}",
	    "version", &file->version,
	    "namespace", &file->ns,
	    "description", &file->description,
	    "use", &uses);

	if (file->version != 1) {
		rpc_set_last_errorf(EINVAL, "Invalid IDL version, should be 1");
		return (-1);
	}

	if (uses != NULL) {
		rpc_array_apply(uses, ^(size_t idx, rpc_object_t value) {
			g_ptr_array_add(file->uses, g_strdup(
			    rpc_string_get_string_ptr(value)));
			return ((bool)true);
		});
	}

	return (ret >= 3 ? 0 : -1);
}

struct rpct_typei *
rpct_instantiate_type(const char *decl, struct rpct_typei *parent,
    struct rpct_type *ptype, struct rpct_file *origin)
{
	GError *err = NULL;
	GMatchInfo *match = NULL;
	GPtrArray *splitvars = NULL;
	struct rpct_type *type = NULL;
	struct rpct_typei *ret = NULL;
	struct rpct_typei *subtype;
	char *decltype = NULL;
	char *declvars = NULL;
	int found_proxy_type = -1;

	debugf("instantiating type %s", decl);

	if (rpct_instance_regex == NULL) {
		rpc_set_last_errorf(ENXIO, "Typing not initialized");
		return (NULL);
	}

	if (!g_regex_match(rpct_instance_regex, decl, 0, &match)) {
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

	if (type != NULL && !type->generic) {
		/*
		 * Non-generic types can be cached, try looking
		 * up in the cache
		 */

		ret = g_hash_table_lookup(context->typei_cache, decltype);
		if (ret != NULL) {
			g_free(decltype);
			g_match_info_free(match);
			return (rpct_typei_retain(ret));
		}
	}

	if (type == NULL) {
		struct rpct_typei *cur = parent;

		debugf("type %s not found, maybe it's a generic variable",
		    decltype);

		while (cur != NULL) {
			/* Maybe it's a type variable? */
			if (cur->type->generic) {
				subtype = g_hash_table_lookup(
				    cur->specializations, decltype);

				if (subtype) {
					ret = subtype;
					goto done;
				}
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
				ret = subtype;
				goto done;
			}
		}

		rpc_set_last_errorf(EINVAL, "Type %s not found", decl);
		goto error;
	}

	ret = g_malloc0(sizeof(*ret));
	ret->refcnt = 1;
	ret->type = type;
	ret->parent = parent;
	ret->specializations = g_hash_table_new_full(g_str_hash, g_str_equal,
	    g_free, (GDestroyNotify)rpct_typei_release);
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
			const char *vartype = g_ptr_array_index(splitvars, i);
			const char *var = g_ptr_array_index(
			    type->generic_vars, i);

			subtype = rpct_instantiate_type(vartype, ret, ptype,
			    origin);
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
		rpct_typei_release(ret);
		ret = NULL;
	}

done:
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

	if (ret != NULL && ret->type != NULL && !ret->type->generic) {
		rpct_typei_retain(ret);
		g_hash_table_insert(context->typei_cache,
		    g_strdup(ret->canonical_form), ret);
	}

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
rpct_interface_free(struct rpct_interface *iface)
{

	printf("freeing %s\n", iface->name);
	g_free(iface->name);
	g_free(iface->description);
	g_hash_table_destroy(iface->members);
}

static void
rpct_if_member_free(struct rpct_if_member *member)
{

	g_free(member->description);
	g_ptr_array_free(member->arguments, true);
	if (member->result != NULL)
		rpct_typei_release(member->result);

	g_free(member);
}

static void
rpct_member_free(struct rpct_member *member)
{

	g_free(member->name);
	g_free(member->description);
	rpct_typei_release(member->type);
	g_hash_table_unref(member->constraints);
	g_free(member);
}

#if 0
static inline bool
rpct_type_is_fully_specialized(struct rpct_typei *inst)
{

	if (!inst->type->generic)
		return (true);

	return (g_hash_table_size(inst->specializations)
	    == inst->type->generic_vars->len);
}
#endif

static inline struct rpct_typei *
rpct_unwind_typei(struct rpct_typei *typei)
{
	struct rpct_typei *current = typei;

	while (current) {
		if (current->type->clazz == RPC_TYPING_TYPEDEF ||
		    current->type->clazz == RPC_TYPING_CONTAINER) {
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
	g_ptr_array_add(variables, g_strndup(&decl[istart],
	    (gsize)(i - istart)));
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

	while (g_hash_table_iter_next(&iter, (gpointer *)&key,
	    (gpointer *)&value)) {
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
	const char *filename;
	struct rpct_file *file;
	__block int ret = -1;

	g_hash_table_iter_init(&iter, context->files);

	while (g_hash_table_iter_next(&iter, (gpointer *)&filename,
	    (gpointer *)&file)) {

		debugf("looking for %s in %s", name, filename);

		rpc_dictionary_apply(file->body,
		    ^(const char *key, rpc_object_t value) {
			GMatchInfo *m;
			g_autofree char *type_name = NULL;
			g_autofree char *full_name = NULL;

			if (!g_regex_match(rpct_type_regex, key, 0, &m)) {
				g_match_info_free(m);
				return ((bool)true);
			}
			type_name = g_match_info_fetch(m, 2);
			full_name = file->ns != NULL
			    ? g_strdup_printf("%s.%s", file->ns, type_name)
			    : g_strdup(type_name);

			if (g_strcmp0(full_name, name) == 0) {
				*decl = key;
				*result = value;
				*filep = file;
				ret = 0;
				g_match_info_free(m);
				return ((bool)false);
			}

			g_match_info_free(m);
			return ((bool)true);
		});
	}

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
	const char *value_type = NULL;
	const char *description = "";
	char *decltype = NULL;
	char *declname = NULL;
	char *declvars = NULL;
	const char *type_def = NULL;
	GMatchInfo *match = NULL;
	rpc_object_t members = NULL;
	int ret = 0;

	g_assert_nonnull(decl);
	g_assert_nonnull(obj);
	debugf("reading type \"%s\"", decl);

	rpc_object_unpack(obj, "{s,s,s,s,v}",
	    "inherits", &inherits,
	    "description", &description,
	    "type", &type_def,
	    "value-type", &value_type,
	    "members", &members);

	if (inherits != NULL) {
		parent = rpct_find_type_fuzzy(inherits, file);
		if (parent == NULL) {
			rpc_set_last_errorf(ENOENT,
			    "Cannot find parent type: %s", inherits);
			return (-1);
		}
	}

	if (!g_regex_match(rpct_type_regex, decl, 0, &match)) {
		rpc_set_last_errorf(EINVAL, "Syntax error: %s", decl);
		ret = -1;
		goto done;
	}

	if (g_match_info_get_match_count(match) < 2) {
		rpc_set_last_errorf(EINVAL, "Syntax error: %s", decl);
		ret = -1;
		goto done;
	}

	decltype = g_match_info_fetch(match, 1);
	declname = g_match_info_fetch(match, 2);
	declvars = g_match_info_fetch(match, 4);

	typename = file->ns != NULL
	    ? g_strdup_printf("%s.%s", file->ns, declname)
	    : g_strdup(declname);

	/* If type already exists, do nothing */
	if (g_hash_table_contains(context->types, typename)) {
		g_free(typename);
		ret = 0;
		goto done;
	}

	type = g_malloc0(sizeof(*type));
	type->origin = g_strdup_printf("%s:%zu", file->path,
	    rpc_get_line_number(obj));
	type->name = typename;
	type->file = file;
	type->parent = parent;
	type->members = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
	    NULL);
	type->constraints = g_hash_table_new_full(g_str_hash, g_str_equal,
	    g_free, (GDestroyNotify)rpc_release_impl);
	type->description = g_strdup(description);
	type->generic_vars = g_ptr_array_new_with_free_func(g_free);

	handler = rpc_find_class_handler(decltype, (rpct_class_t)-1);
	if (handler == NULL) {
		rpc_set_last_errorf(EINVAL, "Unknown class handler: %s",
		    decltype);
		rpct_type_free(type);
		g_free(typename);
		ret = -1;
		goto done;
	}

	type->clazz = handler->id;

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
			g_hash_table_insert(type->members, g_strdup(key),
			    value);
	}

	/* Read member list */
	if (members != NULL) {
		bool stop = rpc_dictionary_apply(members, ^(const char *key,
		    rpc_object_t value) {
			struct rpct_member *m;

			m = handler->member_fn(key, value, type);
			if (m == NULL)
				return ((bool)false);

			g_hash_table_insert(type->members,
			    g_strdup(key), m);
			return ((bool)true);
		});

		if (stop) {
			rpct_type_free(type);
			ret = -1;
			goto done;
		}
	}

	if (type_def != NULL) {
		type->definition = rpct_instantiate_type(type_def, NULL,
		    type, file);

		g_assert_nonnull(type->definition);
	}

	if (value_type != NULL) {
		type->value_type = rpct_instantiate_type(value_type, NULL,
		    type, file);

		g_assert_nonnull(type->value_type);
	}

	if (!g_hash_table_insert(context->types, g_strdup(type->name), type))
		g_assert_not_reached();

	debugf("inserted type %s", declname);
done:
	g_match_info_free(match);
	if (declvars != NULL)
		g_free(declvars);
	if (declname != NULL)
		g_free(declname);
	if (decltype != NULL)
		g_free(decltype);

	return (ret);
}

static int
rpct_read_property(struct rpct_file *file, struct rpct_interface *iface,
    const char *decl, rpc_object_t obj)
{
	struct rpct_if_member *prop;
	GMatchInfo *match = NULL;
	char *name = NULL;
	const char *description = NULL;
	const char *type = NULL;
	bool read_only = false;
	bool read_write = false;
	bool write_only = false;
	bool notify = false;
	int ret = -1;

	g_assert_nonnull(decl);
	g_assert_nonnull(obj);

	if (rpct_check_fields(obj, "description", "type", "read-only",
	    "read-write", "write-only", "notify", NULL) != 0)
		return (-1);

	rpc_object_unpack(obj, "{s,s,b,b,b,b}",
	    "description", &description,
	    "type", &type,
	    "read-only", &read_only,
	    "read-write", &read_write,
	    "write-only", &write_only,
	    "notify", &notify);

	if (!type) {
		rpc_set_last_errorf(EINVAL, "Property %s has no type defined",
		    name);
		goto error;
	}

	if (!g_regex_match(rpct_property_regex, decl, 0, &match)) {
		rpc_set_last_errorf(EINVAL, "Cannot parse: %s", decl);
		goto error;
	}

	if (g_match_info_get_match_count(match) < 1) {
		rpc_set_last_errorf(EINVAL, "Cannot parse: %s", decl);
		goto error;
	}

	name = g_match_info_fetch(match, 1);
	prop = g_malloc0(sizeof(*prop));
	prop->member.rim_name = g_strdup(name);
	prop->member.rim_type = RPC_MEMBER_PROPERTY;
	prop->description = g_strdup(description);

	if (!read_only && !write_only && !read_write) {
		rpc_set_last_errorf(EINVAL, "Property %s has no access "
		    "rights defined", name);
		goto error;
	}

	prop->result = rpct_instantiate_type(type, NULL, NULL, file);
	if (prop->result == NULL)
		goto error;

	g_hash_table_insert(iface->members, g_strdup(name), prop);
	ret = 0;

error:
	g_match_info_free(match);
	if (name != NULL)
		g_free(name);
	return (ret);
}

static int
rpct_read_event(struct rpct_file *file, struct rpct_interface *iface,
    const char *decl, rpc_object_t obj)
{
	struct rpct_if_member *evt;
	GMatchInfo *match = NULL;
	char *name = NULL;
	const char *description = NULL;
	const char *type = NULL;
	int ret = -1;

	g_assert_nonnull(decl);
	g_assert_nonnull(obj);

	if (rpct_check_fields(obj, "description", "type", NULL) != 0)
		return (-1);

	rpc_object_unpack(obj, "{s,s}",
	    "description", &description,
	    "type", &type);

	if (type == NULL) {
		rpc_set_last_errorf(EINVAL, "Event %s has no type defined",
		    name);
		goto error;
	}

	if (!g_regex_match(rpct_event_regex, decl, 0, &match)) {
		rpc_set_last_errorf(EINVAL, "Cannot parse: %s", decl);
		goto error;
	}

	if (g_match_info_get_match_count(match) < 1) {
		rpc_set_last_errorf(EINVAL, "Cannot parse: %s", decl);
		goto error;
	}

	name = g_match_info_fetch(match, 1);
	evt = g_malloc0(sizeof(*evt));
	evt->member.rim_name = g_strdup(name);
	evt->member.rim_type = RPC_MEMBER_EVENT;
	evt->description = g_strdup(description);

	if (type)
		evt->result = rpct_instantiate_type(type, NULL, NULL, file);

	if (evt->result == NULL)
		goto error;

	g_hash_table_insert(iface->members, g_strdup(name), evt);
	ret = 0;

error:
	g_match_info_free(match);
	if (name != NULL)
		g_free(name);
	return (ret);
}

static int
rpct_read_method(struct rpct_file *file, struct rpct_interface *iface,
    const char *decl, rpc_object_t obj)
{
	int ret = -1;
	struct rpct_if_member *method = NULL;
	GError *err = NULL;
	GMatchInfo *match = NULL;
	char *name = NULL;
	const char *description = "";
	const char *returns_type;
	rpc_object_t args = NULL;
	rpc_object_t returns = NULL;
	ret = -1;

	g_assert_nonnull(decl);
	g_assert_nonnull(obj);
	debugf("reading <%s> from file %s", decl, file->path);

	if (rpct_check_fields(obj, "description", "args", "return", NULL) != 0)
		goto error;

	rpc_object_unpack(obj, "{s,v,v}",
	    "description", &description,
	    "args", &args,
	    "return", &returns);

	if (!g_regex_match(rpct_method_regex, decl, 0, &match)) {
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
		if (rpc_array_apply(args, ^(size_t idx, rpc_object_t i) {
			const char *arg_name = NULL;
			const char* arg_type = NULL;
			bool opt = false;
			struct rpct_argument *arg;
			struct rpct_typei *arg_inst;

			rpc_object_unpack(i, "{s,s,b}",
			    "name", &arg_name,
			    "type", &arg_type,
			    "optional", &opt);

			if (arg_name == NULL) {
				rpc_set_last_errorf(EINVAL,
				    "Required 'name' field in argument %zu "
				    "of %s missing", idx, name);
				return ((bool)false);
			}

			if (arg_type == NULL) {
				rpc_set_last_errorf(EINVAL,
				    "Required 'type' field in argument %zu "
				    "of %s missing", idx, name);
				return ((bool)false);
			}

			arg_inst = rpct_instantiate_type(arg_type, NULL, NULL,
			    file);
			if (arg_inst == NULL)
				return ((bool)false);

			arg = g_malloc0(sizeof(*arg));
			arg->opt = opt;
			arg->name = g_strdup(arg_name);
			arg->description = g_strdup(rpc_dictionary_get_string(
			    i, "description"));
			arg->type = arg_inst;
			g_ptr_array_add(method->arguments, arg);
			return ((bool)true);
		}))
			goto error;
	}

	if (returns != NULL) {
		if (rpc_object_unpack(returns, "{type:s}", &returns_type) < 1) {
			rpc_set_last_errorf(EINVAL,
			    "Missing 'type' field in returns clause "
			    " of method %s", name);
			goto error;
		}

		method->result = rpct_instantiate_type(returns_type, NULL,
		    NULL, file);

		if (method->result == NULL)
			goto error;
	}

	method->description = g_strdup(description);

	g_hash_table_insert(iface->members, g_strdup(name), method);
	ret = 0;
	goto done;

error:
	if (method != NULL) {
		g_ptr_array_free(method->arguments, true);
		g_free(method);
		if (name != NULL)
			g_free(name);
	}

done:
	if (err != NULL)
		g_error_free(err);

	if (match != NULL)
		g_match_info_free(match);

	return (ret);
}

static int
rpct_read_interface(struct rpct_file *file, const char *decl, rpc_object_t obj)
{
	struct rpct_interface *iface;
	GMatchInfo *match = NULL;
	char *name = NULL;
	bool result;
	int ret = 0;

	if (!g_regex_match(rpct_interface_regex, decl, 0, &match)) {
		g_match_info_free(match);
		return (-1);
	}

	if (g_match_info_get_match_count(match) < 1) {
		g_match_info_free(match);
		return (-1);
	}

	iface = g_malloc0(sizeof(*iface));
	iface->origin = g_strdup_printf("%s:%zu", file->path,
	    rpc_get_line_number(obj));
	iface->name = g_match_info_fetch(match, 1);
	iface->members = g_hash_table_new_full(g_str_hash, g_str_equal,
	    g_free, (GDestroyNotify)rpct_if_member_free);
	iface->description = g_strdup(rpc_dictionary_get_string(obj,
	    "description"));

	if (file->ns) {
		name = iface->name;
		iface->name = g_strdup_printf("%s.%s", file->ns, name);
		g_free(name);
	}
	g_match_info_free(match);

	if (g_hash_table_contains(context->interfaces, iface->name))
		goto abort;

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

	if (result) {
		ret = -1;
		goto abort;
	}

	g_hash_table_insert(context->interfaces, iface->name, iface);
	g_hash_table_insert(file->interfaces, iface->name, iface);
	return (ret);

abort:
	g_free(iface->origin);
	g_free(iface->description);
	g_free(iface->name);
	g_free(iface);
	return (ret);
}

int
rpct_read_idl(const char *name, rpc_object_t idl)
{
	struct rpct_file *file;

	file = g_malloc0(sizeof(*file));
	file->body = rpc_retain(idl);
	file->path = g_strdup(name);
	file->uses = g_ptr_array_new_with_free_func(g_free);
	file->types = g_hash_table_new(g_str_hash, g_str_equal);
	file->interfaces = g_hash_table_new(g_str_hash, g_str_equal);

	if (rpct_read_meta(file, rpc_dictionary_get_value(idl, "meta")) < 0) {
		rpct_file_free(file);
		return (-1);
	}

	g_hash_table_insert(context->files, g_strdup(name), file);
	return (0);
}

int
rpct_read_file(const char *path)
{
	char *contents;
	size_t length;
	rpc_auto_object_t obj = NULL;
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

	return (rpct_read_idl(path, obj));
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
	while (g_hash_table_iter_next(&iter, (gpointer *)&key,
	    (gpointer *)&value)) {
		v = rpc_find_validator(typename, key);
		if (v == NULL) {
			rpct_add_error(errctx, NULL, "Validator %s not found",
			    key);
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
	struct rpct_typei *raw_typei;
	bool valid;

	raw_typei = rpct_unwind_typei(typei);

	/* Step 1: is it typed at all? */
	if (obj->ro_typei == NULL) {
		/* Can only be builtin type */
		if (g_strcmp0(raw_typei->canonical_form, "any") == 0)
			goto step3;

		if (g_strcmp0(raw_typei->canonical_form, "nullptr") == 0) {
			if (obj->ro_type == RPC_TYPE_NULL)
				goto step3;
		}

		if (g_strcmp0(rpc_get_type_name(obj->ro_type),
		    raw_typei->canonical_form) == 0)
			goto step3;

		rpct_add_error(errctx, NULL,
		    "Incompatible type %s, should be %s",
		    rpc_get_type_name(obj->ro_type),
		    raw_typei->canonical_form);
		return (false);
	}

	/* Step 2: check type */
	if (!rpct_type_is_compatible(raw_typei, obj->ro_typei)) {
		rpct_add_error(errctx, NULL,
		    "Incompatible type %s, should be %s",
		    obj->ro_typei->canonical_form,
		    typei->canonical_form);

		valid = false;
		goto done;
	}

step3:
	handler = rpc_find_class_handler(NULL, typei->type->clazz);
	g_assert_nonnull(handler);

	/* Step 3: run per-class validator */
	valid = handler->validate_fn(typei, obj, errctx);

done:
	return (valid);
}

bool
rpct_validate_args(struct rpct_if_member *func, rpc_object_t args,
    rpc_object_t *errors)
{
	struct rpct_validation_error *err;
	__block struct rpct_error_context errctx;
	__block bool valid = true;
	guint i;

	if (func->arguments == NULL)
		return (true);

	errctx.path = "";
	errctx.errors = g_ptr_array_new();

	rpc_array_apply(args, ^(size_t idx, rpc_object_t i) {
		struct rpct_argument *arg;

		if (idx >= func->arguments->len) {
			valid = false;
			rpct_add_error(&errctx, NULL,
			    "Too many method arguments: %d, should be %d",
			    rpc_array_get_count(args), func->arguments->len);
			return ((bool)false);
		}

		arg = g_ptr_array_index(func->arguments, idx);
		if (!rpct_validate_instance(arg->type, i, &errctx)) {
			valid = false;
			return ((bool)false);
		}

		return ((bool)true);
	});

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

	g_ptr_array_free(errctx.errors, false);
	return (valid);
}

bool
rpct_validate_return(struct rpct_if_member *func, rpc_object_t result,
    rpc_object_t *errors)
{
	if (func->result == NULL)
		return (true);

	return (rpct_validate(func->result, result, errors));
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

	g_ptr_array_free(errctx.errors, false);
	return (valid);
}

rpc_object_t
rpct_pre_call_hook(void *cookie, rpc_object_t args)
{
	struct rpc_call *ic = cookie;
	struct rpct_if_member *member;
	char *msg;
	rpc_object_t errors;

	g_assert(ic->rc_type == RPC_INBOUND_CALL);
	member = rpct_find_if_member(ic->rc_interface, ic->rc_method_name);
	if (member == NULL)
		return (NULL);

	if (!rpct_validate_args(member, args, &errors)) {
		msg = g_strdup_printf("Validation failed: %zu errors",
		    rpc_array_get_count(errors));

		rpc_function_error_ex(cookie,
		    rpc_error_create(EINVAL, msg, errors));
		g_free(msg);
	}

	return (NULL);
}


rpc_object_t
rpct_post_call_hook(void *cookie, rpc_object_t result)
{
	struct rpc_call *ic = cookie;
	struct rpct_if_member *member;
	rpc_object_t errors;

	g_assert(ic->rc_type == RPC_INBOUND_CALL);
	member = rpct_find_if_member(ic->rc_interface, ic->rc_method_name);
	if (member == NULL)
		return (NULL);

	if (!rpct_validate_return(member, result, &errors)) {
		rpc_function_error_ex(cookie, rpc_error_create(EINVAL,
		    "Return value validation failed", errors));
	}

	return (NULL);
}

void
rpct_allow_idl_download(rpc_context_t context)
{
	rpc_instance_register_interface(context->rcx_root,
	    RPCT_TYPING_INTERFACE, rpct_typing_vtable, NULL);
}

int
rpct_download_idl(rpc_connection_t conn)
{
	rpc_call_t call;
	rpc_object_t result;
	rpc_object_t body;
	const char *name;
	int ret = 0;

	call = rpc_connection_call(conn, "/", RPCT_TYPING_INTERFACE,
	    "download", NULL, NULL);
	if (call == NULL)
		return (-1);

next:
	rpc_call_wait(call);

	switch (rpc_call_status(call)) {
	case RPC_CALL_MORE_AVAILABLE:
		result = rpc_call_result(call);
		if (rpc_object_unpack(result, "{s,v}",
		    "name", &name,
		    "body", &body) < 2) {
			ret = -1;
			break;
		}

		if (rpct_read_idl(name, body) < 0)
			ret = -1;

		rpc_call_continue(call, true);
		goto next;

	case RPC_CALL_ENDED:
		break;

	case RPC_CALL_ERROR:
	case RPC_CALL_ABORTED:
		ret = -1;
		break;

	default:
		g_assert_not_reached();
	}

	rpc_call_free(call);
	rpct_load_types_cached();
	return (ret);
}

int
rpct_init(bool load_system_types)
{
	rpct_type_t type;
	const char **b;

	/* Don't initialize twice */
	if (context != NULL)
		return (0);

	/* Compile all the regexes */
	rpct_instance_regex = g_regex_new(INSTANCE_REGEX, 0,
	    G_REGEX_MATCH_NOTEMPTY, NULL);
	rpct_interface_regex =  g_regex_new(INTERFACE_REGEX, 0,
	    G_REGEX_MATCH_NOTEMPTY, NULL);
	rpct_property_regex = g_regex_new(PROPERTY_REGEX, 0,
	    G_REGEX_MATCH_NOTEMPTY, NULL);
	rpct_type_regex = g_regex_new(TYPE_REGEX, 0,
	    G_REGEX_MATCH_NOTEMPTY, NULL);
	rpct_method_regex = g_regex_new(METHOD_REGEX, 0,
	    G_REGEX_MATCH_NOTEMPTY, NULL);
	rpct_event_regex = g_regex_new(EVENT_REGEX, 0,
	    G_REGEX_MATCH_NOTEMPTY, NULL);

	context = g_malloc0(sizeof(*context));
	context->files = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
	    (GDestroyNotify)rpct_file_free);
	context->types = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
	    (GDestroyNotify)rpct_type_free);
	context->interfaces = g_hash_table_new_full(g_str_hash, g_str_equal,
	    NULL, (GDestroyNotify)rpct_interface_free);
	context->typei_cache = g_hash_table_new_full(g_str_hash, g_str_equal,
	    g_free, (GDestroyNotify)rpct_typei_release);

	for (b = builtin_types; *b != NULL; b++) {
		type = g_malloc0(sizeof(*type));
		type->name = g_strdup(*b);
		type->clazz = RPC_TYPING_BUILTIN;
		type->members = g_hash_table_new_full(g_str_hash, g_str_equal,
		    g_free, (GDestroyNotify)rpct_member_free);
		type->constraints = g_hash_table_new_full(g_str_hash,
		    g_str_equal, g_free, (GDestroyNotify)rpc_release_impl);
		type->description = g_strdup_printf("Builtin %s type", *b);
		type->generic_vars = g_ptr_array_new();
		g_hash_table_insert(context->types, g_strdup(type->name), type);
	}

	/* Load system-wide types */
	if (load_system_types)
		return (rpct_load_types_dir("/usr/local/share/idl"));

	return (0);
}

void
rpct_free(void)
{

	g_hash_table_unref(context->files);
	g_free(context);
}

rpct_typei_t
rpct_typei_retain(rpct_typei_t typei)
{

	g_atomic_int_inc(&typei->refcnt);
	return (typei);
}

void
rpct_typei_release(rpct_typei_t typei)
{
	if (!g_atomic_int_dec_and_test(&typei->refcnt))
		return;

	if (typei->specializations != NULL)
		g_hash_table_destroy(typei->specializations);

	g_free(typei->canonical_form);
	g_free(typei);
}

int
rpct_load_types(const char *path)
{
	struct rpct_file *file;
	rpc_object_t error;
	char *errmsg;
	bool fail;

	file = g_hash_table_lookup(context->files, path);
	if (file == NULL) {
		if (rpct_read_file(path) != 0)
			return (-1);
	}

	if (file == NULL)
		g_assert_not_reached();

	if (file->loaded)
		return (-1);

	fail = rpc_dictionary_apply(file->body, ^bool(const char *key,
	    rpc_object_t v) {
		if (g_strcmp0(key, "meta") == 0)
			return (true);

		if (g_str_has_prefix(key, "interface")) {
			if (rpct_read_interface(file, key, v) != 0)
				return ((bool)false);

			return (true);
	    	}

		if (rpct_read_type(file, key, v) != 0)
			return (false);

		return (true);
	});

	if (fail) {
		error = rpc_get_last_error();
		errmsg = g_strdup_printf("%s: %s", path,
		    rpc_error_get_message(error));
		rpc_set_last_error(rpc_error_get_code(error), errmsg,
		    rpc_error_get_extra(error));

#ifdef RPC_TRACE
		rpc_trace("ERROR", "rpct_load_types", rpc_get_last_error());
#endif
		return (-1);
	}

	return (0);
}

int
rpct_load_types_dir(const char *path)
{
	GDir *dir;
	GPtrArray *files;
	GError *error = NULL;
	const char *name;
	char *s;
	guint i;

	dir = g_dir_open(path, 0, &error);
	if (dir == NULL) {
		rpc_set_last_gerror(error);
		g_error_free(error);
		return (-1);
	}

	files = g_ptr_array_new_with_free_func((GDestroyNotify)g_free);

	for (;;) {
		name = g_dir_read_name(dir);
		if (name == NULL)
			break;

		s = g_build_filename(path, name, NULL);
		if (g_file_test(s, G_FILE_TEST_IS_DIR)) {
			rpct_load_types_dir(s);
			continue;
		}

		if (!g_str_has_suffix(name, ".yaml"))
			continue;

		if (rpct_read_file(s) != 0) {
			g_free(s);
			continue;
		}

		g_ptr_array_add(files, s);
	}

	g_dir_close(dir);

	for (i = 0; i < files->len; i++) {
		s = g_ptr_array_index(files, i);
		rpct_load_types(s);
	}

	g_ptr_array_free(files, true);
	return (0);
}

int
rpct_load_types_stream(int fd)
{

	rpc_set_last_errorf(ENOTSUP, "Not implemented");
	return (-1);
}

int
rpct_load_types_cached(void)
{
	GHashTableIter iter;
	struct rpct_file *file;

	g_hash_table_iter_init(&iter, context->files);
	while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&file)) {
		if (file->loaded)
			continue;

		if (rpct_load_types(file->path) != 0)
			return (-1);
	}

	return (0);
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
rpct_type_get_origin(rpct_type_t type)
{

	return (type->origin);
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

bool
rpct_typei_get_proxy(rpct_typei_t typei)
{

	return (typei->proxy);
}

const char *
rpct_typei_get_proxy_variable(rpct_typei_t typei)
{

	return (typei->variable);
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
rpct_interface_get_origin(rpct_interface_t iface)
{

	return (iface->origin);
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
rpct_argument_get_name(rpct_argument_t arg)
{

	return (arg->name);
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
	    (gpointer *)&value)) {
		if (!applier(value)) {
			flag = true;
			break;
		}
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

rpct_interface_t
rpct_find_interface(const char *name)
{
	struct rpct_interface *iface;

	iface = g_hash_table_lookup(context->interfaces, name);
	if (iface == NULL) {
		rpc_set_last_errorf(ENOENT, "Interface not found");
		return (NULL);
	}

	return (iface);
}

rpct_if_member_t
rpct_find_if_member(const char *interface, const char *member)
{
	struct rpct_if_member *ret;
	struct rpct_interface *iface;

	iface = rpct_find_interface(interface);
	if (iface == NULL)
		return (NULL);

	ret = g_hash_table_lookup(iface->members, member);
	if (ret == NULL) {
		rpc_set_last_errorf(ENOENT, "Member not found");
		return (NULL);
	}

	return (ret);
}

rpc_object_t
rpct_serialize(rpc_object_t object)
{
	const struct rpct_class_handler *handler;
	rpct_class_t clazz;
	rpc_object_t cont;

	if (context == NULL)
		return (rpc_retain(object));

	if (object->ro_typei == NULL ||
	    object->ro_typei->type->clazz == RPC_TYPING_BUILTIN) {
		/* Try recursively */
		if (rpc_get_type(object) == RPC_TYPE_DICTIONARY) {
			cont = rpc_dictionary_create();
			cont->ro_typei = rpct_new_typei("dictionary");
			rpc_dictionary_apply(object,
			    ^(const char *key, rpc_object_t v) {
				rpc_dictionary_steal_value(cont, key,
				    rpct_serialize(v));
				return ((bool)true);
			});

			return (cont);
		} else if (rpc_get_type(object) == RPC_TYPE_ARRAY) {
			cont = rpc_array_create();
			cont->ro_typei = rpct_new_typei("array");
			rpc_array_apply(object, ^(size_t idx, rpc_object_t v) {
				rpc_array_append_stolen_value(cont,
				    rpct_serialize(v));
				return ((bool)true);
			});

			return (cont);
		} else {
			cont = rpc_copy(object);
			cont->ro_typei = rpct_new_typei(
			    rpc_get_type_name(rpc_get_type(object)));
			return (cont);
		}
	}

	clazz = object->ro_typei->type->clazz;
	handler = rpc_find_class_handler(NULL, clazz);
	g_assert_nonnull(handler);

	return (handler->serialize_fn(object));
}

rpc_object_t
rpct_deserialize(rpc_object_t object)
{
	const struct rpct_class_handler *handler;
	const char *typedecl;
	rpc_type_t objtype = rpc_get_type(object);
	rpct_typei_t typei;
	rpct_class_t clazz;
	rpc_object_t result;

	if (context == NULL)
		return (rpc_retain(object));

	if (object->ro_typei != NULL)
		return (rpc_retain(object));

	if (objtype == RPC_TYPE_DICTIONARY) {
		typedecl = rpc_dictionary_get_string(object, RPCT_TYPE_FIELD);
		if (typedecl == NULL)
			goto builtin;

		typei = rpct_new_typei(typedecl);
		if (typei == NULL)
			return (rpc_null_create());

		clazz = typei->type->clazz;
		handler = rpc_find_class_handler(NULL, clazz);
		g_assert_nonnull(handler);

		result = handler->deserialize_fn(object);
		result->ro_typei = typei;
		return (result);
	}

builtin:
	handler = rpc_find_class_handler(NULL, RPC_TYPING_BUILTIN);
	g_assert_nonnull(handler);

	result = handler->deserialize_fn(object);
	result->ro_typei = rpct_new_typei(rpc_get_type_name(objtype));
	return (result);
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
rpct_add_error(struct rpct_error_context *ctx, rpc_object_t extra,
    const char *fmt, ...)
{
	va_list ap;
	struct rpct_validation_error *err;

	va_start(ap, fmt);
	err = g_malloc0(sizeof(*err));
	err->path = g_strdup(ctx->path);
	err->message = g_strdup_vprintf(fmt, ap);
	err->extra = extra;
	va_end(ap);

	g_ptr_array_add(ctx->errors, err);
}
