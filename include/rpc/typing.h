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

#ifdef __cplusplus
extern "C" {
#endif

#define	RPCT_TYPING_INTERFACE	"com.twoporeguys.librpc.Typing"
#define	RPCT_TYPE_FIELD		"%type"
#define	RPCT_VALUE_FIELD	"%value"

struct rpct_type;
struct rpct_typei;
struct rpct_member;
struct rpct_interface;
struct rpct_if_member;
struct rpct_argument;

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
 * Represents an interface.
 */
typedef struct rpct_interface *rpct_interface_t;

/**
 * Represents a function argument.
 */
typedef struct rpct_argument *rpct_argument_t;

/**
 * Represents an interface member (method, property or event).
 */
typedef struct rpct_if_member *rpct_if_member_t;

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

/**
 * Block type used as a callback in @ref rpct_types_apply.
 */
typedef bool (^rpct_type_applier_t)(rpct_type_t);

/**
 * Block type used as a callback in @ref rpct_members_apply.
 */
typedef bool (^rpct_member_applier_t)(rpct_member_t);

/**
 * Block type used as a callback in @ref rpct_interface_apply.
 */
typedef bool (^rpct_interface_applier_t)(rpct_interface_t);

/**
 * Block type used as a callback in @ref rpct_if_member_apply.
 */
typedef bool (^rpct_if_member_applier_t)(rpct_if_member_t);

#define	RPCT_TYPE_APPLIER(_fn, _arg)					\
	^(rpct_type_t _type) {						\
		return ((bool)_fn(_arg, _type));			\
	}

#define	RPCT_INTERFACE_APPLIER(_fn, _arg)				\
	^(rpct_interface_t _iface) {					\
		return ((bool)_fn(_arg, _iface));			\
	}

#define	RPCT_MEMBER_APPLIER(_fn, _arg)					\
	^(rpct_member_t _member) {					\
		return ((bool)_fn(_arg, _member));			\
	}

#define	RPCT_IF_MEMBER_APPLIER(_fn, _arg)				\
	^(rpct_if_member_t _if_member) {				\
		return ((bool)_fn(_arg, _if_member));			\
	}

/**
 * Initializes RPC type system
 *
 * @return 0 on success, -1 on error
 */
int rpct_init(void);

/**
 * Cleans up all the state associated with RPC type system.
 */
void rpct_free(void);

/**
 * Reads IDL file without parsing it. @ref rpct_load_types must be called
 * on the same path again to load the associated types.
 *
 * @param path Path to the IDL file
 * @return 0 on success, -1 on error
 */
int rpct_read_file(const char *path);

/**
 * Reads IDL object without parsing it. @ref rpct_load_types must be called
 * on the same name again to load the associated types.
 *
 * @param name Name of the IDL blob
 * @param idl IDL blob contents
 * @return 0 on success, -1 on error
 */
int rpct_read_idl(const char *name, rpc_object_t idl);

/**
 * Loads type information from an interface definition file.
 *
 * @param path Path of the IDL file
 * @return 0 on success, -1 on error
 */
int rpct_load_types(const char *path);

/**
 *
 * @param path
 * @return
 */
int rpct_load_types_dir(const char *path);

/**
 * Loads type information from an interface definition stream.
 *
 * File descriptor is closed once all definitions have been
 * read from it or error happened.
 *
 * @param fd IDL stream file descriptor
 * @return 0 on success, -1 on error
 */
int rpct_load_types_stream(int fd);

/**
 * Returns the type name.
 *
 * @param type Type handle
 * @return Type name
 */
const char *rpct_type_get_name(rpct_type_t type);

/**
 * Returns the place where type was defined.
 *
 * Returned string is in form `file.yaml:line_number`
 *
 * @param iface Interface handle
 * @return Origin string
 */
const char *rpct_type_get_origin(rpct_type_t iface);

/**
 * Returns the module name type belongs to.
 *
 * @param type Type handle
 * @return Module name
 */
const char *rpct_type_get_module(rpct_type_t type);

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

/**
 * Returns the type definition (underlying type).
 *
 * This function returns the underlying type definition of a typedef.
 * Returns NULL for other type classes.
 *
 * @param type Type handle
 * @return Type definition handle or NULL.
 */
rpct_typei_t rpct_type_get_definition(rpct_type_t type);

/**
 * Returns a number of generic variables a type defines.
 *
 * @param type Type handle
 * @return Number of generic variables (0 for non-generic types)
 */
int rpct_type_get_generic_vars_count(rpct_type_t type);

/**
 * Returns name of n-th generic variable.
 *
 * Returns NULL if index is out of the bounds.
 *
 * @param type Type handle
 * @param index Generic variable index
 * @return Generic variable name
 */
const char *rpct_type_get_generic_var(rpct_type_t type, int index);

/**
 * Looks up type member by name.
 *
 * @param type Type handle
 * @param name Member name
 * @return Member handle or NULL if not found
 */
rpct_member_t rpct_type_get_member(rpct_type_t type, const char *name);

/**
 * Increments reference count of a type instance handle.
 *
 * @param typei Type instance handle
 * @return Same as the passed argument
 */
rpct_typei_t rpct_typei_retain(rpct_typei_t typei);

/**
 * Decrements reference count of a type instance handle.
 *
 * If the reference count reaches 0, type instance handle is freed.
 *
 * @param typei Type instance handle
 */
void rpct_typei_release(rpct_typei_t typei);

/**
 * Returns base type of a type instance @p typei.
 *
 * @param typei Type instance handle
 * @return Type handle
 */
rpct_type_t rpct_typei_get_type(rpct_typei_t typei);

/**
 * Returns the type instance passed as a generic variable to the @p typei.
 *
 * @param typei Type instance handle
 * @param name Generic variable name
 * @return Type instance handle or @p NULL
 */
rpct_typei_t rpct_typei_get_generic_var(rpct_typei_t typei, const char *name);

/**
 * Returns the type declaration string ("canonical form").
 *
 * @param typei Type instance handle
 * @return Canonical type declaration string
 */
const char *rpct_typei_get_canonical_form(rpct_typei_t typei);

/**
 * Returns type instance handler of a structure or union member.
 *
 * @param typei Type instance handle
 * @param member Member handle
 * @return Type instance handle or NULL in case of error
 */
rpct_typei_t rpct_typei_get_member_type(rpct_typei_t typei,
    rpct_member_t member);

/**
 * Returns the name of a member.
 *
 * @param member Member handle
 * @return Member name
 */
const char *rpct_member_get_name(rpct_member_t member);

/**
 * Returns the description of a member.
 *
 * @param member Member handle
 * @return Description text or NULL.
 */
const char *rpct_member_get_description(rpct_member_t member);

/**
 * Returns the type of a member.
 *
 * This functions returns NULL for enum members, because they're untyped.
 *
 * @param member Member handle
 * @return Type instance handle representing member type or NULL
 */
rpct_typei_t rpct_member_get_typei(rpct_member_t member);

/**
 * Returns the interface name.
 *
 * @param iface Interface handle
 * @return
 */
const char *rpct_interface_get_name(rpct_interface_t iface);

/**
 * Returns the place where interface was defined.
 *
 * Returned string is in form `file.yaml:line_number`
 *
 * @param iface Interface handle
 * @return Origin string
 */
const char *rpct_interface_get_origin(rpct_interface_t iface);

/**
 * Returns the interface description.
 *
 * @param iface Interface handle
 * @return Interface description or @p NULL
 */
const char *rpct_interface_get_description(rpct_interface_t iface);

/**
 * Returns the interface member type.
 *
 * @param member Interface member handle
 * @return Interface member type
 */
enum rpc_if_member_type rpct_if_member_get_type(rpct_if_member_t member);

/**
 * Returns the interface member name.
 *
 * @param member Interface member handle
 * @return Interface member name
 */
const char *rpct_if_member_get_name(rpct_if_member_t member);

/**
 * Returns the interface member description.
 *
 * @param member Interface member handle
 * @return Interface member description or NULL
 */
const char *rpct_if_member_get_description(rpct_if_member_t member);

/**
 * Returns the type instance handle representing return type of a function.
 *
 * @param method Method handle
 * @return Return type instance handle
 */
rpct_typei_t rpct_method_get_return_type(rpct_if_member_t method);

/**
 * Returns number of arguments a function takes.
 *
 * @param method Method handle
 * @return Number of arguments
 */
int rpct_method_get_arguments_count(rpct_if_member_t method);

/**
 * Returns argument handle for n-th argument of a function.
 *
 * @param func Function handle
 * @param index Argument index
 * @return Argument handle or NULL if index is out of bounds
 */
rpct_argument_t rpct_method_get_argument(rpct_if_member_t method, int index);

/**
 * Returns type instance of the property.
 *
 * @param prop Property handle
 * @return Type instance handle of the property
 */
rpct_typei_t rpct_property_get_type(rpct_if_member_t prop);

/**
 * Returns name associated with the argument.
 *
 * @param arg Argument handle
 * @return Argument name
 */
const char *rpct_argument_get_name(rpct_argument_t arg);

/**
 * Returns description string associated with the argument.
 *
 * @param arg Argument handle
 * @return Description string or NULL
 */
const char *rpct_argument_get_description(rpct_argument_t arg);

/**
 * Returns type instance of the argument.
 *
 * @param arg Argument handle
 * @return Type instance handle
 */
rpct_typei_t rpct_argument_get_typei(rpct_argument_t arg);

/**
 * Iterates over the defined types.
 *
 * @param applier
 * @return @p true if iteration was terminated, otherwise @p false
 */
bool rpct_types_apply(rpct_type_applier_t applier);

/**
 * Iterates over the members of a given type.
 * @param type
 * @param applier
 * @return @p true if iteration was terminated, otherwise @p false
 */
bool rpct_members_apply(rpct_type_t type, rpct_member_applier_t applier);

/**
 * Iterates over the defined functions.
 *
 * @param applier
 * @return @p true if iteration was terminated, otherwise @p false
 */
bool rpct_interface_apply(rpct_interface_applier_t applier);

/**
 * Iterates over members of an interface.
 *
 * @param applier Callback function
 * @return @p true if iteration was terminated, otherwise @p false
 */
bool rpct_if_member_apply(rpct_interface_t iface, rpct_if_member_applier_t applier);

/**
 * Finds interface @p name.
 *
 * @param name Interface name
 * @return Interface handle
 */
rpct_interface_t rpct_find_interface(const char *name);

/**
 * Finds the member @p member of interface @p interface.
 *
 * @param interface Interface name
 * @param member Member name
 * @return Interface member handle
 */
rpct_if_member_t rpct_find_if_member(const char *interface, const char *member);

/**
 * Creates a new type instance from provided declaration.
 *
 * @param decl Type declaration
 * @return Type instance handle or NULL in case of error
 */
rpct_typei_t rpct_new_typei(const char *decl);

/**
 * Creates a new object of the specified type.
 *
 * @param decl Type declaration
 * @param object Contents
 * @return Typed object
 */
rpc_object_t rpct_new(const char *decl, rpc_object_t object);

/**
 * Creates a new object of the specified type.
 *
 * @param typei Type instance handle
 * @param object Contents
 * @return Typed object
 */
rpc_object_t rpct_newi(rpct_typei_t typei, rpc_object_t object);

/**
 * Binds specified type to an existing object.
 *
 * @param typei Type instance handle
 * @param object Object to be typed
 * @return Typed object
 */
rpc_object_t rpct_set_typei(rpct_typei_t typei, rpc_object_t object);

/**
 * Looks up type by name.
 *
 * @param name Type name
 * @return Type handle or NULL if not found
 */
rpct_type_t rpct_get_type(const char *name);

/**
 * Returns type instance handle associated with an object.
 *
 * @param instance RPC object instance
 * @return Type instance handle or NULL
 */
rpct_typei_t rpct_get_typei(rpc_object_t instance);

/**
 *
 * @param instance
 * @return
 */
rpc_object_t rpct_get_value(rpc_object_t instance);

/**
 *
 * @param object
 * @param value
 */
void rpct_set_value(rpc_object_t object, const char *value);

/**
 * Serializes object hierarchy preserving type information.
 *
 * @param object Object to serialize
 * @return Object with encoded type information
 */
rpc_object_t rpct_serialize(rpc_object_t object);

/**
 * Deserializes object hierarchy previously serialized with rpct_serialize()
 *
 * @param object
 * @return
 */
rpc_object_t rpct_deserialize(rpc_object_t object);

/**
 * Validates set of arguments for a given function.
 *
 * @param func
 * @param args
 * @param errors
 * @return
 */
bool rpct_validate_args(struct rpct_if_member *func, rpc_object_t args,
    rpc_object_t *errors);

/**
 * Validates a return value from a given function.
 *
 * @param func
 * @param result
 * @param errors
 * @return
 */
bool rpct_validate_return(struct rpct_if_member *func, rpc_object_t result,
    rpc_object_t *errors);

/**
 * Validates object against given type instance.
 *
 * @param typei
 * @param obj
 * @param errors
 * @return
 */
bool rpct_validate(rpct_typei_t typei, rpc_object_t obj, rpc_object_t *errors);

/**
 *
 * @param cookie
 * @param args
 * @return
 */
rpc_object_t rpct_pre_call_hook(void *cookie, rpc_object_t args);

/**
 *
 * @param cookie
 * @param args
 * @return
 */
rpc_object_t rpct_post_call_hook(void *cookie, rpc_object_t args);

/**
 * XXX
 *
 * @param context
 */
void rpct_allow_idl_download(rpc_context_t context);

/**
 * XXX
 *
 * @param conn Connection handle
 * @return 0 on success, -1 on error
 */
int rpct_download_idl(rpc_connection_t conn);

#ifdef __cplusplus
}
#endif

#endif /* LIBRPC_TYPING_H */
