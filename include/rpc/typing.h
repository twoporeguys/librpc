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

#ifndef LIBRPC_TYPING_H
#define LIBRPC_TYPING_H

#include <rpc/object.h>

/**
 * @file typing.h
 */

#define	RPCT_REALM_FIELD	"%realm"
#define	RPCT_TYPE_FIELD		"%type"
#define	RPCT_VALUE_FIELD	"%value"

struct rpct_type;
struct rpct_typei;
struct rpct_member;

/**
 * Represents a type, as defined in the interface definition file.
 *
 * rpct_type_t represents a defined type, that is - an unspecialized
 * type. Unspecialized means that if a type has any generic variables
 * (eg. Type<A, B>), A and B are type placeholders.
 *
 * Examples of unspecialized types:
 * - string
 * - NonGenericStructOne
 * - GenericStructTwo<T>
 * - GenericTypedef<V>
 * - HashMap<K, V>
 */
typedef struct rpct_type *rpct_type_t;

/**
 * Represents a specialized type.
 *
 * rpct_typei_t represents a specialized type, that is - a possibly
 * generic type with its generic variables specialized with actual types.
 *
 * One special case here is a partially specialized type. A partially
 * specialized type may be present as a structure member, union branch
 * or typedef body. "partially" means that some of the type variables
 * might be specialized, but some others might not.
 *
 * Examples of specialized types:
 * - string
 * - NonGenericStructOne
 * - GenericStructTwo<int64>
 * - GenericTypedef<string>
 * - HashMap<string, double>
 *
 * Examples of partially specialized types:
 * - GenericStructTwo<T>
 * - HashMap<K, V>
 * - HashMap<K, double>
 */
typedef struct rpct_typei *rpct_typei_t;

/**
 * Represents a structure member or a union branch.
 */
typedef struct rpct_member *rpct_member_t;

/**
 * A type class.
 */
typedef enum {
	RPC_TYPING_STRUCT,		/**< A structure */
	RPC_TYPING_UNION,		/**< A union */
	RPC_TYPING_ENUM,		/**< An enum */
	RPC_TYPING_TYPEDEF,		/**< A type alias */
	RPC_TYPING_BUILTIN		/**< A builtin type */
} rpct_class_t;

typedef bool (^rpct_type_applier_t)(rpct_type_t);
typedef bool (^rpct_member_applier_t)(rpct_member_t);

#define	RPCT_TYPE_APPLIER(_fn, _arg)					\
	^(rpct_type_t _type) {						\
		return ((bool)_fn(_arg, _type));			\
	}

#define	RPCT_MEMBER_APPLIER(_fn, _arg)					\
	^(rpct_member_t _member) {					\
		return ((bool)_fn(_arg, _member));			\
	}

/**
 * Initializes RPC type system
 *
 * @return 0 on success, -1 on error
 */
int rpct_init(void);
void rpct_free(void);
int rpct_load_types(const char *path);
int rpct_load_types_stream(int fd);

/**
 * Returns the current realm name.
 *
 * @return Realm name or NULL if not set
 */
const char *rpct_get_realm(void);

/**
 * Sets the current realm name.
 *
 * Returns -1 and sets errno to ENOENT if given realm cannot be found.
 *
 * @param realm Realm name
 * @return 0 on success, -1 on error
 */
int rpct_set_realm(const char *realm);

/**
 * Returns the type name.
 *
 * @param type Type handle
 * @return Type name
 */
const char *rpct_type_get_name(rpct_type_t type);

/**
 * Returns the name of the realm type belongs to.
 *
 * @param type Type handle
 * @return Realm name
 */
const char *rpct_type_get_realm(rpct_type_t type);

/**
 * Returns the type description, as read from interface definition file.
 *
 * @param type Type handle
 * @return Description string (or empty string if not defined)
 */
const char *rpct_type_get_description(rpct_type_t type);

/**
 * Returns the type "parent" in the inheritance chain.
 *
 * @param type Type handle
 * @return Base type or NULL
 */
rpct_type_t rpct_type_get_parent(rpct_type_t type);

/**
 * Returns the type class.
 *
 * @param type Type handle
 * @return Type class
 */
rpct_class_t rpct_type_get_class(rpct_type_t type);

rpct_typei_t rpct_type_get_definition(rpct_type_t type);

int rpct_type_get_generic_vars_count(rpct_type_t type);
rpct_member_t rpct_type_get_member(rpct_type_t, const char *name);
rpct_type_t rpct_typei_get_type(rpct_typei_t typei);
rpct_typei_t rpct_typei_get_generic_var(rpct_typei_t typei, const char *name);
const char *rpct_typei_get_canonical_form(rpct_typei_t typei);
rpct_typei_t rpct_typei_get_member_type(rpct_typei_t typei, rpct_member_t member);

const char *rpct_member_get_name(rpct_member_t member);
const char *rpct_member_get_description(rpct_member_t member);
rpct_typei_t rpct_member_get_typei(rpct_member_t member);

bool rpct_types_apply(rpct_type_applier_t applier);
bool rpct_members_apply(rpct_type_t type, rpct_member_applier_t applier);

rpct_typei_t rpct_new_typei(const char *decl);
rpc_object_t rpct_new(const char *decl, const char *realm, rpc_object_t object);
rpc_object_t rpct_newi(rpct_typei_t typei, rpc_object_t object);

rpct_type_t rpct_get_type(const char *name);
rpct_typei_t rpct_get_typei(rpc_object_t instance);
rpc_object_t rpct_get_value(rpc_object_t instance);
void rpct_set_value(rpc_object_t object, const char *value);

rpc_object_t rpct_serialize(rpc_object_t object);
rpc_object_t rpct_deserialize(rpc_object_t object);

#endif /* LIBRPC_TYPING_H */
