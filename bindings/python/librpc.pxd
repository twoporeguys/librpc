#
# Copyright 2017 Two Pore Guys, Inc.
# All rights reserved
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted providing that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES LOSS OF USE, DATA, OR PROFITS OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
# IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

from libc.stdint cimport *


ctypedef bint (*rpc_dictionary_applier_f)(void *arg, const char *key, rpc_object_t value)
ctypedef bint (*rpc_array_applier_f)(void *arg, size_t index, rpc_object_t value)
ctypedef int (*rpc_array_cmp_f)(void *arg, rpc_object_t o1, rpc_object_t o2)
ctypedef void (*rpc_binary_destructor_f)(void *buffer)
ctypedef void (*rpc_binary_destructor_arg_f)(void *arg, void *buffer)
ctypedef void (*rpc_handler_f)(void *arg, const char *path, const char *interface, const char *name, rpc_object_t args)
ctypedef void (*rpc_error_handler_f)(void *arg, rpc_error_code_t code, rpc_object_t args)
ctypedef bint (*rpc_callback_f)(void *arg, rpc_call_t call, rpc_call_status_t status)
ctypedef void (*rpc_bus_event_handler_f)(void *arg, rpc_bus_event_t, rpc_bus_node *)
ctypedef bint (*rpct_type_applier_f)(void *arg, rpct_type_t type)
ctypedef bint (*rpct_interface_applier_f)(void *arg, rpct_interface_t iface)
ctypedef bint (*rpct_member_applier_f)(void *arg, rpct_member_t member)
ctypedef bint (*rpct_if_member_applier_f)(void *arg, rpct_if_member_t if_member)
ctypedef rpc_object_t (*rpc_property_getter_f)(void *cookie)
ctypedef void (*rpc_property_setter_f)(void *cookie, rpc_object_t value)
ctypedef void (*rpc_property_handler_f)(void *cookie, rpc_object_t value)
ctypedef rpc_object_t (*rpc_query_cb_f)(void *arg, rpc_object_t object)


cdef extern from "rpc/object.h" nogil:
    ctypedef struct rpc_object_t:
        pass

    ctypedef enum rpc_error_code_t:
        RPC_INVALID_RESPONSE
        RPC_CONNECTION_TIMEOUT
        RPC_CONNECTION_CLOSED
        RPC_CALL_TIMEOUT
        RPC_SPURIOUS_RESPONSE
        RPC_LOGOUT
        RPC_OTHER

    ctypedef enum rpc_type_t:
        RPC_TYPE_NULL
        RPC_TYPE_BOOL
        RPC_TYPE_UINT64
        RPC_TYPE_INT64
        RPC_TYPE_DOUBLE
        RPC_TYPE_DATE
        RPC_TYPE_STRING
        RPC_TYPE_BINARY
        RPC_TYPE_FD
        RPC_TYPE_ERROR
        RPC_TYPE_DICTIONARY
        RPC_TYPE_ARRAY

    void *RPC_DICTIONARY_APPLIER(rpc_dictionary_applier_f fn, void *arg)
    void *RPC_ARRAY_APPLIER(rpc_array_applier_f fn, void *arg)
    void *RPC_ARRAY_CMP(rpc_array_cmp_f fn, void *arg)
    void *RPC_BINARY_DESTRUCTOR_ARG(rpc_binary_destructor_arg_f fn, void *arg)

    rpc_object_t rpc_get_last_error()

    rpc_object_t rpc_retain(rpc_object_t object)
    int rpc_release_impl(rpc_object_t object)
    rpc_object_t rpc_copy(rpc_object_t object)
    bint rpc_equal(rpc_object_t o1, rpc_object_t o2)
    size_t rpc_hash(rpc_object_t object)
    char *rpc_copy_description(rpc_object_t object)
    rpc_type_t rpc_get_type(rpc_object_t object)
    const char *rpc_get_type_name(rpc_type_t type)
    void rpc_release(rpc_object_t object)

    rpc_object_t rpc_null_create()
    rpc_object_t rpc_bool_create(bint value)
    bint rpc_bool_get_value(rpc_object_t xbool)
    rpc_object_t rpc_int64_create(int64_t value)
    int64_t rpc_int64_get_value(rpc_object_t xint)
    rpc_object_t rpc_uint64_create(uint64_t value)
    uint64_t rpc_uint64_get_value(rpc_object_t xuint)
    rpc_object_t rpc_double_create(double value)
    double rpc_double_get_value(rpc_object_t xdouble)
    rpc_object_t rpc_date_create(int64_t interval)
    rpc_object_t rpc_date_create_from_current()
    int64_t rpc_date_get_value(rpc_object_t xdate)
    rpc_object_t rpc_data_create(const void *bytes, size_t length, void *destructor)
    size_t rpc_data_get_length(rpc_object_t xdata)
    const void *rpc_data_get_bytes_ptr(rpc_object_t xdata)
    size_t rpc_data_get_bytes(rpc_object_t xdata, void *buffer, size_t off,
        size_t length)
    rpc_object_t rpc_string_create(const char *string)
    size_t rpc_string_get_length(rpc_object_t xstring)
    const char *rpc_string_get_string_ptr(rpc_object_t xstring)
    rpc_object_t rpc_fd_create(int fd)
    int rpc_fd_dup(rpc_object_t xfd)
    int rpc_fd_get_value(rpc_object_t xfd)

    rpc_object_t rpc_error_create(int code, const char *msg, rpc_object_t extra)
    rpc_object_t rpc_error_create_with_stack(int code, const char *msg, rpc_object_t extra, rpc_object_t stack)
    int rpc_error_get_code(rpc_object_t error)
    const char *rpc_error_get_message(rpc_object_t error)
    rpc_object_t rpc_error_get_extra(rpc_object_t error)
    void rpc_error_set_extra(rpc_object_t error, rpc_object_t extra)
    rpc_object_t rpc_error_get_stack(rpc_object_t error)

    bint rpc_equal(rpc_object_t o1, rpc_object_t o2)
    char *rpc_copy_description(rpc_object_t object)

    rpc_object_t rpc_array_create()
    void rpc_array_set_value(rpc_object_t array, size_t index, rpc_object_t value)
    bint rpc_array_apply(rpc_object_t array, void *applier)
    void rpc_array_append_value(rpc_object_t array, rpc_object_t value)
    rpc_object_t rpc_array_get_value(rpc_object_t array, size_t index)
    size_t rpc_array_get_count(rpc_object_t array)
    void rpc_array_remove_index(rpc_object_t array, size_t index)
    void rpc_array_remove_all(rpc_object_t array)

    rpc_object_t rpc_dictionary_create()
    rpc_object_t rpc_dictionary_get_value(rpc_object_t dictionary,
        const char *key)
    void rpc_dictionary_set_value(rpc_object_t dictionary, const char *key,
        rpc_object_t value)
    bint rpc_dictionary_apply(rpc_object_t dictionary, void *applier)
    size_t rpc_dictionary_get_count(rpc_object_t dictionary)
    void rpc_dictionary_remove_key(rpc_object_t dictionary, const char *key)
    void rpc_dictionary_remove_all(rpc_object_t dictionary)


cdef extern from "rpc/connection.h" nogil:
    ctypedef enum rpc_call_status_t:
        RPC_CALL_IN_PROGRESS
        RPC_CALL_STREAM_START
        RPC_CALL_MORE_AVAILABLE
        RPC_CALL_DONE
        RPC_CALL_ERROR
        RPC_CALL_ABORTED
        RPC_CALL_ENDED

    ctypedef struct rpc_connection_t:
        pass

    ctypedef struct rpc_call_t:
        pass

    void *RPC_HANDLER(rpc_handler_f fn, void *arg)
    void *RPC_ERROR_HANDLER(rpc_error_handler_f fn, void *arg)
    void *RPC_CALLBACK(rpc_callback_f fn, void *arg)
    void *RPC_PROPERTY_HANDLER(rpc_property_handler_f fn, void *arg)

    rpc_connection_t rpc_connection_create(void *cookie, rpc_object_t params)
    int rpc_connection_close(rpc_connection_t conn)
    rpc_context_t rpc_connection_get_context(rpc_connection_t conn)
    int rpc_connection_set_context(rpc_connection_t conn, rpc_context_t ctx)
    int rpc_connection_subscribe_event(rpc_connection_t conn, const char *name)
    int rpc_connection_unsubscribe_event(rpc_connection_t conn, const char *name)
    rpc_object_t rpc_connection_call_sync(rpc_connection_t conn,
        const char *path, const char *interface, const char *method, ...)
    rpc_call_t rpc_connection_call(rpc_connection_t conn, const char *path,
        const char *interface, const char *name, rpc_object_t args, void *callback)
    int rpc_connection_send_event(rpc_connection_t conn, const char *path,
        const char *interface, const char *name, rpc_object_t args)
    void rpc_connection_set_event_handler(rpc_connection_t conn, void *handler)
    void rpc_connection_set_error_handler(rpc_connection_t conn, void *handler)
    rpc_call_status_t rpc_call_status(rpc_call_t call)
    int rpc_call_wait(rpc_call_t call)
    int rpc_call_continue(rpc_call_t call, bint sync)
    int rpc_call_abort(rpc_call_t call)
    int rpc_call_success(rpc_call_t call)
    rpc_object_t rpc_call_result(rpc_call_t call)
    void rpc_call_free(rpc_call_t call)

    void *rpc_connection_register_event_handler(rpc_connection_t conn, const char *path,
        const char *interface, const char *name, void *handler)
    void *rpc_connection_watch_property(rpc_connection_t conn,
        const char *path, const char *interface, const char *property,
        void *handler)
    void rpc_connection_unregister_event_handler(rpc_connection_t conn,
        void *cookie)


cdef extern from "rpc/service.h" nogil:
    ctypedef rpc_object_t (*rpc_function_f)(void *cookie, rpc_object_t args)

    void *RPC_PROPERTY_GETTER(rpc_property_getter_f getter)
    void *RPC_PROPERTY_SETTER(rpc_property_setter_f setter)

    cdef struct rpc_if_member:
        const char *rim_name

    cdef enum rpc_if_member_type:
        RPC_MEMBER_EVENT
        RPC_MEMBER_PROPERTY
        RPC_MEMBER_METHOD

    ctypedef struct rpc_context_t:
        pass

    ctypedef struct rpc_instance_t:
        pass

    rpc_context_t rpc_context_create()
    void rpc_context_free(rpc_context_t context)
    int rpc_context_register_instance(rpc_context_t context, rpc_instance_t instance)
    int rpc_context_register_func(rpc_context_t context, const char *interface,
        const char *name, void *arg, rpc_function_f func)
    int rpc_context_unregister_member(rpc_context_t context, const char *interface, const char *name)

    void *rpc_function_get_arg(void *cookie)
    void rpc_function_respond(void *cookie, rpc_object_t object)
    void rpc_function_error(void *cookie, int code, const char *message, ...)
    void rpc_function_error_ex(void *cookie, rpc_object_t exception)
    int rpc_function_start_stream(void *cookie)
    int rpc_function_yield(void *cookie, rpc_object_t fragment)
    void rpc_function_produce(void *cookie, rpc_object_t fragment)
    void rpc_function_end(void *cookie)

    void *rpc_property_get_arg(void *cookie)
    void rpc_property_error(void *cookie, int code, const char *fmt)

    rpc_instance_t rpc_instance_new(void *arg, const char *path)
    void rpc_instance_set_description(rpc_instance_t instance, const char *description)
    void *rpc_instance_get_arg(rpc_instance_t instance)
    const char *rpc_instance_get_path(rpc_instance_t instance)
    int rpc_instance_register_interface(rpc_instance_t instance, const char *interface, rpc_if_member *vtable, void *arg)
    int rpc_instance_register_method(rpc_instance_t instance, const char *interface,
        const char *name, void *arg, void *fn)
    int rpc_instance_register_func(rpc_instance_t instance, const char *interface,
        const char *name, void *arg, rpc_function_f fn)
    int rpc_instance_register_property(rpc_instance_t instance, const char *interface, const char *name,
        void *arg, void *getter, void *setter)
    void rpc_instance_property_changed(rpc_instance_t instance,
        const char *interface, const char *name, rpc_object_t value)
    void rpc_instance_free(rpc_instance_t instance)

    int rpc_instance_register(rpc_context_t context, rpc_instance_t instance)
    int rpc_instance_unregister(const char *path)


cdef extern from "rpc/client.h" nogil:
    ctypedef struct rpc_client_t:
        pass

    rpc_client_t rpc_client_create(const char *uri, rpc_object_t params)
    rpc_connection_t rpc_client_get_connection(rpc_client_t client)
    void rpc_client_close(rpc_client_t client)


cdef extern from "rpc/server.h" nogil:
    ctypedef struct rpc_server:
        pass

    ctypedef rpc_server *rpc_server_t

    rpc_server_t rpc_server_create_ex(const char *uri, rpc_context_t context, rpc_object_t params);
    int rpc_server_resume(rpc_server_t server)
    int rpc_server_close(rpc_server_t server)
    void rpc_server_broadcast_event(rpc_server_t server, const char *path, const char *interface, const char *name, rpc_object_t args)
    int rpc_server_sd_listen(rpc_context_t context, rpc_server_t **servers, rpc_object_t *rest)

cdef extern from "rpc/bus.h" nogil:
    ctypedef enum rpc_bus_event_t:
        RPC_BUS_ATTACHED
        RPC_BUS_DETACHED

    cdef struct rpc_bus_node:
        const char *rbn_name
        const char *rbn_description
        const char *rbn_serial
        uint32_t rbn_address

    void *RPC_BUS_EVENT_HANDLER(rpc_bus_event_handler_f fn, void *arg)

    int rpc_bus_open()
    int rpc_bus_close()
    int rpc_bus_ping(const char *name)
    int rpc_bus_enumerate(rpc_bus_node **result)
    int rpc_bus_free_result(rpc_bus_node *result)
    void rpc_bus_register_event_handler(void *handler)
    void rpc_bus_unregister_event_handler()


cdef extern from "rpc/serializer.h" nogil:
    bint rpc_serializer_exists(const char *serializer)
    rpc_object_t rpc_serializer_load(const char *serializer, const void *frame, size_t len)
    int rpc_serializer_dump(const char *serializer, rpc_object_t obj, void **framep, size_t *lenp)


cdef extern from "rpc/query.h" nogil:
    cdef struct rpc_query_params:
        bint single
        bint count
        uint64_t offset
        uint64_t limit
        bint reverse
        void *sort
        void *callback

    ctypedef struct rpc_query_iter:
        pass

    ctypedef rpc_query_iter *rpc_query_iter_t
    ctypedef rpc_query_params *rpc_query_params_t

    void *RPC_QUERY_CB(rpc_query_cb_f fn, void *arg)

    rpc_object_t rpc_query_get(rpc_object_t object, const char *path, rpc_object_t default_val)
    void rpc_query_set(rpc_object_t object, const char *path, rpc_object_t value, bint steal)
    void rpc_query_delete(rpc_object_t object, const char *path)
    bint rpc_query_contains(rpc_object_t object, const char *path)
    rpc_query_iter_t rpc_query(rpc_object_t object, rpc_query_params_t params, rpc_object_t rules)
    bint rpc_query_next(rpc_query_iter_t iter, rpc_object_t *chunk)
    void rpc_query_iter_free(rpc_query_iter_t iter)


cdef extern from "rpc/typing.h" nogil:
    ctypedef struct rpct_type_t:
        pass

    ctypedef struct rpct_typei_t:
        pass

    ctypedef struct rpct_member_t:
        pass

    ctypedef struct rpct_interface_t:
        pass

    ctypedef struct rpct_if_member_t:
        pass

    ctypedef struct rpct_argument_t:
        pass

    ctypedef enum rpct_class_t:
        RPC_TYPING_STRUCT
        RPC_TYPING_UNION
        RPC_TYPING_ENUM
        RPC_TYPING_TYPEDEF
        RPC_TYPING_CONTAINER
        RPC_TYPING_BUILTIN

    void *RPCT_TYPE_APPLIER(rpct_type_applier_f fn, void *arg)
    void *RPCT_MEMBER_APPLIER(rpct_member_applier_f fn, void *arg)
    void *RPCT_INTERFACE_APPLIER(rpct_interface_applier_f fn, void *arg)
    void *RPCT_IF_MEMBER_APPLIER(rpct_if_member_applier_f fn, void *args)

    void rpct_init(bint load_system_types)
    int rpct_read_file(const char *path)
    int rpct_load_types(const char *path)
    int rpct_load_types_dir(const char *path)
    int rpct_download_idl(rpc_connection_t conn)

    rpct_interface_t rpct_find_interface(const char *name)
    rpct_if_member_t rpct_find_if_member(const char *interface, const char *member)

    bint rpct_types_apply(void *applier)
    bint rpct_members_apply(rpct_type_t type, void *applier)
    bint rpct_interface_apply(void *applier)
    bint rpct_if_member_apply(rpct_interface_t iface, void *applier)

    const char *rpct_type_get_name(rpct_type_t type)
    const char *rpct_type_get_module(rpct_type_t type)
    const char *rpct_type_get_description(rpct_type_t type)
    rpct_type_t rpct_type_get_parent(rpct_type_t type)
    rpct_typei_t rpct_type_get_definition(rpct_type_t type)

    rpct_class_t rpct_type_get_class(rpct_type_t type)
    int rpct_type_get_generic_vars_count(rpct_type_t type)
    const char *rpct_type_get_generic_var(rpct_type_t type, int index)

    bint rpct_typei_get_proxy(rpct_typei_t typei)
    const char *rpct_typei_get_proxy_variable(rpct_typei_t typei)
    rpct_type_t rpct_typei_get_type(rpct_typei_t typei)
    const char *rpct_typei_get_canonical_form(rpct_typei_t typei)
    rpct_typei_t rpct_typei_get_generic_var(rpct_typei_t typei, const char *name)

    rpct_typei_t rpct_typei_get_member_type(rpct_typei_t typei, rpct_member_t member)

    const char *rpct_member_get_name(rpct_member_t member)
    const char *rpct_member_get_description(rpct_member_t member)
    rpct_typei_t rpct_member_get_typei(rpct_member_t member)

    const char *rpct_interface_get_name(rpct_interface_t member)
    const char *rpct_interface_get_description(rpct_interface_t member)

    const char *rpct_if_member_get_name(rpct_if_member_t member)
    const char *rpct_if_member_get_description(rpct_if_member_t member)
    rpc_if_member_type rpct_if_member_get_type(rpct_if_member_t member)
    rpct_typei_t rpct_method_get_return_type(rpct_if_member_t method)
    int rpct_method_get_arguments_count(rpct_if_member_t method)
    rpct_argument_t rpct_method_get_argument(rpct_if_member_t method, int index)
    rpct_typei_t rpct_property_get_type(rpct_if_member_t prop)

    const char *rpct_argument_get_name(rpct_argument_t arg)
    const char *rpct_argument_get_description(rpct_argument_t arg)
    rpct_typei_t rpct_argument_get_typei(rpct_argument_t arg)

    rpct_typei_t rpct_get_typei(rpc_object_t instance)
    rpc_object_t rpct_get_value(rpc_object_t instance)

    rpct_typei_t rpct_new_typei(const char *decl)
    rpc_object_t rpct_new(const char *decl, rpc_object_t object)
    rpc_object_t rpct_newi(rpct_typei_t typei, rpc_object_t object)
    rpc_object_t rpct_set_typei(rpct_typei_t typei, rpc_object_t object)

    rpc_object_t rpct_serialize(rpc_object_t object)
    rpc_object_t rpct_deserialize(rpc_object_t object)
    bint rpct_validate(rpct_typei_t typei, rpc_object_t obj, rpc_object_t *errors)


cdef extern from "rpc/rpcd.h" nogil:
    rpc_client_t rpcd_connect_to(const char *rpcd_uri, const char *name)
    int rpcd_register(const char *uri, const char *name, const char *description)


cdef class Object(object):
    cdef rpc_object_t obj
    cdef object ref

    @staticmethod
    cdef wrap(rpc_object_t ptr, bint retain=*)
    cdef rpc_object_t unwrap(self) nogil


cdef class Context(object):
    cdef rpc_context_t context
    cdef bint borrowed
    cdef object methods
    cdef object instances

    @staticmethod
    cdef Context wrap(rpc_context_t ptr)
    cdef rpc_context_t unwrap(self) nogil


cdef class Instance(object):
    cdef readonly Context context
    cdef rpc_instance_t instance
    cdef object properties
    cdef public arg

    @staticmethod
    cdef Instance wrap(rpc_instance_t ptr)
    cdef rpc_instance_t unwrap(self) nogil
    @staticmethod
    cdef rpc_object_t c_property_getter(void *cookie) with gil
    @staticmethod
    cdef void c_property_setter(void *cookie, rpc_object_t value) with gil


cdef class Service(object):
    cdef readonly Instance instance
    cdef object methods
    cdef object properties
    cdef object interfaces


cdef class RemoteObject(object):
    cdef readonly object client
    cdef readonly object path
    cdef readonly object name
    cdef readonly object description
    cdef readonly object interfaces


cdef class RemoteInterface(object):
    cdef readonly object client
    cdef readonly object instance
    cdef readonly object path


cdef class RemoteProperty(object):
    cdef readonly object name
    cdef readonly object interface
    cdef readonly object typed


cdef class RemoteEvent(object):
    cdef object handlers
    cdef readonly object name
    cdef readonly object interface
    cdef readonly object typed

    cdef emit(self, Object args)


cdef class TypeInstance(object):
    cdef rpct_typei_t rpctypei

    @staticmethod
    cdef TypeInstance wrap(rpct_typei_t typei)
    cdef rpct_typei_t unwrap(self) nogil


cdef class Interface(object):
    cdef rpct_interface_t c_iface

    @staticmethod
    cdef Interface wrap(rpct_interface_t ptr)
    cdef rpct_interface_t unwrap(self) nogil
    @staticmethod
    cdef bint c_iter(void *arg, rpct_if_member_t val)


cdef class InterfaceMember(object):
    cdef rpct_if_member_t c_member

    @staticmethod
    cdef wrap(rpct_if_member_t ptr)
    cdef rpct_if_member_t unwrap(self) nogil


cdef class BaseTypingObject(object):
    cdef readonly Object __object__
    cdef readonly TypeInstance __typei__

    @staticmethod
    cdef construct_struct(TypeInstance typei)
    @staticmethod
    cdef construct_union(TypeInstance typei)
    @staticmethod
    cdef construct_enum(TypeInstance typei)


cdef class Call(object):
    cdef readonly Connection connection
    cdef rpc_call_t call

    @staticmethod
    cdef Call wrap(rpc_call_t ptr)
    cdef rpc_call_t unwrap(self) nogil


cdef class Connection(object):
    cdef public bint unpack
    cdef rpc_connection_t connection
    cdef object error_handler
    cdef object event_handler
    cdef object ev_handlers
    cdef bint borrowed

    @staticmethod
    cdef Connection wrap(rpc_connection_t ptr)
    cdef rpc_connection_t unwrap(self) nogil
    @staticmethod
    cdef void c_ev_handler(void *arg, const char *path, const char *inteface, const char *name, rpc_object_t args) with gil
    @staticmethod
    cdef void c_prop_handler(void *arg, rpc_object_t value) with gil
    @staticmethod
    cdef void c_error_handler(void *arg, rpc_error_code_t code, rpc_object_t args) with gil


cdef class Client(Connection):
    cdef rpc_client_t client
    cdef object uri

    @staticmethod
    cdef Client wrap(rpc_client_t ptr)


cdef class Bus(object):
    cdef object event_fn

    @staticmethod
    cdef void c_ev_handler(void *arg, rpc_bus_event_t ev, rpc_bus_node *bn) with gil


cdef class QueryIterator(object):
    cdef rpc_query_iter_t iter
    cdef object sort_cb
    cdef object postprocess_cb
    cdef object cnt
    cdef object unpack

    @staticmethod
    cdef QueryIterator wrap(rpc_query_iter_t iter, object sort, object cb, object unpack)
    @staticmethod
    cdef rpc_object_t c_callback(void *arg, rpc_object_t object)
    @staticmethod
    cdef int c_sort(void *arg, rpc_object_t o1, rpc_object_t o2)
