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
from libcpp cimport bool


cdef extern from "rpc/object.h" nogil:
    ctypedef struct rpc_object:
        pass

    ctypedef rpc_object *rpc_object_t

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
        RPC_TYPE_DICTIONARY
        RPC_TYPE_ARRAY

    rpc_object_t rpc_retain(rpc_object_t object)
    int rpc_release_impl(rpc_object_t object)
    rpc_object_t rpc_copy(rpc_object_t object)
    bool rpc_equal(rpc_object_t o1, rpc_object_t o2)
    size_t rpc_hash(rpc_object_t object)
    char *rpc_copy_description(rpc_object_t object)
    rpc_type_t rpc_get_type(rpc_object_t object)
    void rpc_release(rpc_object_t object)

    rpc_object_t rpc_null_create()
    rpc_object_t rpc_bool_create(bool value)
    bool rpc_bool_get_value(rpc_object_t xbool)
    rpc_object_t rpc_int64_create(int64_t value)
    int64_t rpc_int64_get_value(rpc_object_t xint)
    rpc_object_t rpc_uint64_create(uint64_t value)
    uint64_t rpc_uint64_get_value(rpc_object_t xuint)
    rpc_object_t rpc_double_create(double value)
    double rpc_double_get_value(rpc_object_t xdouble)
    rpc_object_t rpc_date_create(int64_t interval)
    rpc_object_t rpc_date_create_from_current()
    int64_t rpc_date_get_value(rpc_object_t xdate)
    rpc_object_t rpc_data_create(const void *bytes, size_t length, bool copy)
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

    rpc_object_t rpc_array_create()
    void rpc_array_set_value(rpc_object_t array, size_t index, rpc_object_t value)
    void rpc_array_append_value(rpc_object_t array, rpc_object_t value)
    rpc_object_t rpc_array_get_value(rpc_object_t array, size_t index)

    rpc_object_t rpc_dictionary_create()
    rpc_object_t rpc_dictionary_get_value(rpc_object_t dictionary,
        const char *key)
    void rpc_dictionary_set_value(rpc_object_t dictionary, const char *key,
        rpc_object_t value)

cdef extern from "rpc/connection.h" nogil:
    ctypedef void (*rpc_handler_f)(const char *name, rpc_object_t args)
    ctypedef struct rpc_connection:
        pass

    ctypedef struct rpc_call:
        pass

    ctypedef rpc_connection *rpc_connection_t
    ctypedef rpc_call *rpc_call_t

    rpc_connection_t rpc_connection_create(const char *uri, int flags)
    int rpc_connection_close(rpc_connection_t conn)
    int rpc_connection_subscribe_event(rpc_connection_t conn, const char *name)
    int rpc_connection_unsubscribe_event(rpc_connection_t conn, const char *name)
    rpc_object_t rpc_connection_call_sync(rpc_connection_t conn, const char *method, ...)
    void rpc_connection_call_async(rpc_connection_t conn, const char *method, ...)
    rpc_call_t rpc_connection_call(rpc_connection_t conn, const char *name,
        rpc_object_t args)
    int rpc_connection_send_event(rpc_connection_t conn, const char *name,
        rpc_object_t args)
    void rpc_connection_set_event_handler_f(rpc_connection_t conn,
        rpc_handler_f handler)

    int rpc_call_wait(rpc_call_t call)
    int rpc_call_continue(rpc_call_t call, bool sync)
    int rpc_call_abort(rpc_call_t call)
    int rpc_call_success(rpc_call_t call)
    rpc_object_t rpc_call_result(rpc_call_t call)
    void rpc_call_free(rpc_call_t call)


cdef extern from "rpc/service.h" nogil:
    ctypedef rpc_object_t (*rpc_function_f)(void *cookie, rpc_object_t args);
    ctypedef struct rpc_context:
        pass

    ctypedef rpc_context *rpc_context_t

    rpc_context_t rpc_context_create()
    void rpc_context_free(rpc_context_t context)
    int rpc_context_register_method_f(rpc_context_t context, const char *name,
        const char *descr, void *arg, rpc_function_f func)
    int rpc_context_unregister_method(rpc_context_t context, const char *name)

    void *rpc_function_get_arg(void *cookie)
    void rpc_function_respond(void *cookie, rpc_object_t object)
    void rpc_function_error(void *cookie, int code, const char *message, ...)
    void rpc_function_error_ex(void *cookie, rpc_object_t exception)
    void rpc_function_produce(void *cookie, rpc_object_t fragment)
    void rpc_function_end(void *cookie)


cdef extern from "rpc/client.h" nogil:
    ctypedef struct rpc_client:
        pass

    ctypedef rpc_client *rpc_client_t

    rpc_client_t rpc_client_create(const char *uri, int flags)
    rpc_connection_t rpc_client_get_connection(rpc_client_t client)
    void rpc_client_close(rpc_client_t client)


cdef extern from "rpc/server.h" nogil:
    ctypedef struct rpc_server:
        pass

    ctypedef rpc_server *rpc_server_t

    rpc_server_t rpc_server_create(const char *uri, rpc_context_t context);
    int rpc_server_start(rpc_server_t server, bool background);
    int rpc_server_stop(rpc_server_t server);
    int rpc_server_close(rpc_server_t server);