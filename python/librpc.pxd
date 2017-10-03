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
ctypedef void (*rpc_handler_f)(void *arg, const char *name, rpc_object_t args)
ctypedef void (*rpc_error_handler_f)(void *arg, rpc_error_code_t code, rpc_object_t args)
ctypedef bint (*rpc_callback_f)(void *arg, rpc_call_t call, rpc_call_status_t status)
ctypedef void (*rpc_bus_event_handler_f)(void *arg, rpc_bus_event_t, rpc_bus_node *)
ctypedef bint (*rpct_type_applier_f)(void *arg, rpct_type_t type)
ctypedef bint (*rpct_member_applier_f)(void *arg, rpct_member_t member)


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

    rpc_object_t rpc_get_last_error()

    rpc_object_t rpc_retain(rpc_object_t object)
    int rpc_release_impl(rpc_object_t object)
    rpc_object_t rpc_copy(rpc_object_t object)
    bint rpc_equal(rpc_object_t o1, rpc_object_t o2)
    size_t rpc_hash(rpc_object_t object)
    char *rpc_copy_description(rpc_object_t object)
    rpc_type_t rpc_get_type(rpc_object_t object)
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
    rpc_object_t rpc_data_create(const void *bytes, size_t length, bint copy)
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

    rpc_object_t rpc_dictionary_create()
    rpc_object_t rpc_dictionary_get_value(rpc_object_t dictionary,
        const char *key)
    void rpc_dictionary_set_value(rpc_object_t dictionary, const char *key,
        rpc_object_t value)
    bint rpc_dictionary_apply(rpc_object_t dictionary, void *applier)
    size_t rpc_dictionary_get_count(rpc_object_t dictionary)
    void rpc_dictionary_remove_key(rpc_object_t dictionary, const char *key)


cdef extern from "rpc/connection.h" nogil:
    ctypedef enum rpc_call_status_t:
        RPC_CALL_IN_PROGRESS,
        RPC_CALL_MORE_AVAILABLE,
        RPC_CALL_DONE,
        RPC_CALL_ERROR

    ctypedef struct rpc_connection_t:
        pass

    ctypedef struct rpc_call_t:
        pass

    void *RPC_HANDLER(rpc_handler_f fn, void *arg)
    void *RPC_ERROR_HANDLER(rpc_error_handler_f fn, void *arg)
    void *RPC_CALLBACK(rpc_callback_f fn, void *arg)

    rpc_connection_t rpc_connection_create(const char *uri, rpc_object_t params)
    int rpc_connection_close(rpc_connection_t conn)
    int rpc_connection_subscribe_event(rpc_connection_t conn, const char *name)
    int rpc_connection_unsubscribe_event(rpc_connection_t conn, const char *name)
    rpc_object_t rpc_connection_call_sync(rpc_connection_t conn, const char *method, ...)
    rpc_call_t rpc_connection_call(rpc_connection_t conn, const char *name,
        rpc_object_t args, void *callback)
    int rpc_connection_send_event(rpc_connection_t conn, const char *name,
        rpc_object_t args)
    void rpc_connection_set_event_handler(rpc_connection_t conn, void *handler)
    void rpc_connection_set_error_handler(rpc_connection_t conn, void *handler)
    int rpc_call_status(rpc_call_t call)
    int rpc_call_wait(rpc_call_t call)
    int rpc_call_continue(rpc_call_t call, bint sync)
    int rpc_call_abort(rpc_call_t call)
    int rpc_call_success(rpc_call_t call)
    rpc_object_t rpc_call_result(rpc_call_t call)
    void rpc_call_free(rpc_call_t call)

    int rpc_connection_register_event_handler(rpc_connection_t conn, const char *name, void *handler)


cdef extern from "rpc/service.h" nogil:
    ctypedef rpc_object_t (*rpc_function_f)(void *cookie, rpc_object_t args);
    ctypedef struct rpc_context:
        pass

    ctypedef rpc_context *rpc_context_t

    rpc_context_t rpc_context_create()
    void rpc_context_free(rpc_context_t context)
    int rpc_context_register_func(rpc_context_t context, const char *name,
        const char *descr, void *arg, rpc_function_f func)
    int rpc_context_unregister_method(rpc_context_t context, const char *name)

    void *rpc_function_get_arg(void *cookie)
    void rpc_function_respond(void *cookie, rpc_object_t object)
    void rpc_function_error(void *cookie, int code, const char *message, ...)
    void rpc_function_error_ex(void *cookie, rpc_object_t exception)
    int rpc_function_yield(void *cookie, rpc_object_t fragment)
    void rpc_function_produce(void *cookie, rpc_object_t fragment)
    void rpc_function_end(void *cookie)


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

    rpc_server_t rpc_server_create(const char *uri, rpc_context_t context);
    int rpc_server_start(rpc_server_t server, bint background);
    int rpc_server_stop(rpc_server_t server);
    int rpc_server_close(rpc_server_t server);

    void rpc_server_broadcast_event(rpc_server_t server, const char *name, rpc_object_t args)


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
    rpc_object_t rpc_serializer_load(const char *serializer, const void *frame, size_t len)
    int rpc_serializer_dump(const char *serializer, rpc_object_t obj, void **framep, size_t *lenp)


cdef extern from "rpc/typing.h" nogil:
    ctypedef struct rpct_type_t:
        pass

    ctypedef struct rpct_typei_t:
        pass

    ctypedef struct rpct_member_t:
        pass

    ctypedef enum rpct_class_t:
        RPC_TYPING_STRUCT
        RPC_TYPING_UNION
        RPC_TYPING_ENUM
        RPC_TYPING_TYPEDEF
        RPC_TYPING_BUILTIN

    void *RPCT_TYPE_APPLIER(rpct_type_applier_f fn, void *arg)
    void *RPCT_MEMBER_APPLIER(rpct_member_applier_f fn, void *arg)

    void rpct_init()
    int rpct_load_types(const char *path)

    const char *rpct_get_realm()
    void rpct_set_realm(const char *realm)

    void rpct_types_apply(void *applier)
    void rpct_members_apply(rpct_type_t type, void *applier)

    const char *rpct_type_get_name(rpct_type_t type)
    const char *rpct_type_get_realm(rpct_type_t type)
    const char *rpct_type_get_module(rpct_type_t type)
    const char *rpct_type_get_description(rpct_type_t type)
    rpct_type_t rpct_type_get_parent(rpct_type_t type)
    rpct_typei_t rpct_type_get_definition(rpct_type_t type)

    rpct_class_t rpct_type_get_class(rpct_type_t type)
    int rpct_type_get_generic_vars_count(rpct_type_t type)
    const char *rpct_type_get_generic_var(rpct_type_t type, int index)

    rpct_type_t rpct_typei_get_type(rpct_typei_t typei)
    const char *rpct_typei_get_canonical_form(rpct_typei_t typei)
    rpct_typei_t rpct_typei_get_generic_var(rpct_typei_t typei, int index)

    rpct_typei_t rpct_typei_get_member_type(rpct_typei_t typei, rpct_member_t member)

    const char *rpct_member_get_name(rpct_member_t member)
    const char *rpct_member_get_description(rpct_member_t member)
    rpct_typei_t rpct_member_get_typei(rpct_member_t member)

    rpct_typei_t rpct_get_typei(rpc_object_t instance)
    rpc_object_t rpct_get_value(rpc_object_t instance)

    rpct_typei_t rpct_new_typei(const char *decl)
    rpc_object_t rpct_new(const char *decl, const char *realm, rpc_object_t object)
    rpc_object_t rpct_newi(rpct_typei_t typei, rpc_object_t object)

    rpc_object_t rpct_serialize(rpc_object_t object)
    rpc_object_t rpct_deserialize(rpc_object_t object)
    bint rpct_validate(rpct_typei_t typei, rpc_object_t obj, rpc_object_t *errors)


cdef class Object(object):
    cdef rpc_object_t obj

    @staticmethod
    cdef Object init_from_ptr(rpc_object_t ptr)


cdef class Context(object):
    cdef rpc_context_t context
    cdef bint borrowed
    cdef object methods

    @staticmethod
    cdef Context init_from_ptr(rpc_context_t ptr)
    @staticmethod
    cdef rpc_object_t c_cb_function(void *cookie, rpc_object_t args) with gil


cdef class Connection(object):
    cdef rpc_connection_t connection
    cdef object error_handler
    cdef object event_handler
    cdef object ev_handlers
    cdef bint borrowed

    @staticmethod
    cdef Connection init_from_ptr(rpc_connection_t ptr)
    @staticmethod
    cdef void c_ev_handler(const char *name, rpc_object_t args, void *arg) with gil
    @staticmethod
    cdef void c_error_handler(void *arg, rpc_error_code_t code, rpc_object_t args) with gil


cdef class Bus(object):
    cdef object event_fn

    @staticmethod
    cdef void c_ev_handler(void *arg, rpc_bus_event_t ev, rpc_bus_node *bn) with gil
