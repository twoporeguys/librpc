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

import enum
import datetime
cimport defs
from libc.stdint cimport *
from libc.stdlib cimport malloc, free


class ObjectType(enum.IntEnum):
    NIL = defs.RPC_TYPE_NULL
    BOOL = defs.RPC_TYPE_BOOL
    UINT64 = defs.RPC_TYPE_UINT64
    INT64 = defs.RPC_TYPE_INT64
    DOUBLE = defs.RPC_TYPE_DOUBLE
    DATE = defs.RPC_TYPE_DATE
    STRING = defs.RPC_TYPE_STRING
    BINARY = defs.RPC_TYPE_BINARY
    FD = defs.RPC_TYPE_FD
    DICTIONARY = defs.RPC_TYPE_DICTIONARY
    ARRAY = defs.RPC_TYPE_ARRAY


cdef class Object(object):
    cdef defs.rpc_object_t obj

    def __init__(self, value, force_type=None):
        if value is None or force_type == ObjectType.NIL:
            self.obj = defs.rpc_null_create()
            return

        if isinstance(value, bool) or force_type == ObjectType.BOOL:
            self.obj = defs.rpc_bool_create(value)
            return

        if isinstance(value, int) or force_type == ObjectType.INT64:
            self.obj = defs.rpc_int64_create(value)
            return

        if isinstance(value, int) and force_type == ObjectType.UINT64:
            self.obj = defs.rpc_uint64_create(value)
            return

        if isinstance(value, int) and force_type == ObjectType.FD:
            self.obj = defs.rpc_fd_create(value)
            return

        if isinstance(value, str) or force_type == ObjectType.STRING:
            byte_str = value.encode('utf-8')
            self.obj = defs.rpc_string_create(byte_str)
            return

        if isinstance(value, float) or force_type == ObjectType.DOUBLE:
            self.obj = defs.rpc_double_create(value)
            return

        if isinstance(value, datetime.datetime) or force_type == ObjectType.DATE:
            self.obj = defs.rpc_date_create(int(value.timestamp()))
            return

        if isinstance(value, bytearray) or force_type == ObjectType.BINARY:
            self.obj = defs.rpc_data_create(<void *>value, <size_t>len(value), True)
            return

        if isinstance(value, list) or force_type == ObjectType.ARRAY:
            self.obj = defs.rpc_array_create()

            for v in value:
                child = Object(v)
                defs.rpc_array_append_value(self.obj, child.obj)

            return

        if isinstance(value, dict) or force_type == ObjectType.DICTIONARY:
            self.obj = defs.rpc_dictionary_create()

            for k, v in value.items():
                byte_k = k.encode('utf-8')
                child = Object(v)
                defs.rpc_dictionary_set_value(self.obj, byte_k, child.obj)

        raise TypeError(f"Cannot create RPC object - unknown value type: {type(value)}")

    def __dealloc__(self):
        defs.rpc_release(self.obj)

    property value:
        def __get__(self):
            cdef Array array
            cdef Dictionary dictionary
            cdef const char *c_string = NULL
            cdef const uint8_t *c_bytes = NULL
            cdef size_t c_len = 0

            if self.type() == ObjectType.NIL:
                return None

            if self.type() == ObjectType.BOOL:
                return defs.rpc_bool_get_value(self.obj)

            if self.type() == ObjectType.INT64:
                return defs.rpc_int64_get_value(self.obj)

            if self.type() == ObjectType.UINT64:
                return defs.rpc_uint64_get_value(self.obj)

            if self.type() == ObjectType.FD:
                return defs.rpc_fd_get_value(self.obj)

            if self.type() == ObjectType.STRING:
                c_string = defs.rpc_string_get_string_ptr(self.obj)
                c_len = defs.rpc_string_get_length(self.obj)

                return c_string[:c_len].decode('UTF-8')

            if self.type() == ObjectType.DOUBLE:
                return defs.rpc_double_get_value(self.obj)

            if self.type() == ObjectType.DATE:
                return datetime.datetime.utcfromtimestamp(defs.rpc_date_get_value(self.obj))

            if self.type() == ObjectType.BINARY:
                c_bytes = <uint8_t *>defs.rpc_data_get_bytes_ptr(self.obj)
                c_len = defs.rpc_data_get_length(self.obj)

                return <bytes>c_bytes[:c_len]

            if self.type() == ObjectType.ARRAY:
                array = Array.__new__(Array)
                array.obj = self.obj
                defs.rpc_retain(array.obj)
                return array

            if self.type() == ObjectType.DICTIONARY:
                dictionary = Dictionary.__new__(Dictionary)
                dictionary.obj = self.obj
                defs.rpc_retain(dictionary.obj)
                return dictionary

    property type:
        def __get__(self):
            return ObjectType(defs.rpc_get_type(self.obj))


cdef class Array(Object):
    def __init__(self, value, force_type=None):
        if not isinstance(value, list):
            raise TypeError(f"Cannot initialize Array RPC object from {type(value)} type")

        super(Array, self).__init__(value, force_type)

    def __getitem__(self, index):
        cdef Object rpc_value

        rpc_value = Object.__new__(Object)
        rpc_value.obj = defs.rpc_array_get_value(self.obj, index)
        defs.rpc_retain(rpc_value.obj)

        return rpc_value

    def __setitem__(self, index, value):
        cdef Object rpc_value

        rpc_value = Object(value)
        defs.rpc_array_set_value(self.obj, index, rpc_value.obj)

        defs.rpc_retain(rpc_value.obj)

    def append(self, value):
        cdef Object rpc_value

        rpc_value = Object(value)
        defs.rpc_array_append_value(self.obj, rpc_value.obj)

    def extend(self, array):
        cdef Object rpc_value

        for value in array:
            rpc_value = Object(value)
            defs.rpc_array_append_value(self.obj, rpc_value.obj)


cdef class Dictionary(Object):
    def __init__(self, value, force_type=None):
        if not isinstance(value, dict):
            raise TypeError(f"Cannot initialize Dictionary RPC object from {type(value)} type")

        super(Dictionary, self).__init__(value, force_type)

    def __getitem__(self, key):
        cdef Object rpc_value
        byte_key = key.encode('utf-8')

        rpc_value = Object.__new__(Object)
        rpc_value.obj = defs.rpc_dictionary_get_value(self.obj, byte_key)
        defs.rpc_retain(rpc_value.obj)

        return rpc_value

    def __setitem__(self, key, value):
        cdef Object rpc_value
        byte_key = key.encode('utf-8')

        rpc_value = Object(value)
        defs.rpc_dictionary_set_value(self.obj, byte_key, rpc_value.obj)

        defs.rpc_retain(rpc_value.obj)


cdef class Context(object):
    cdef defs.rpc_context_t context

    def __init__(self):
        self.context = defs.rpc_context_create()

    @staticmethod
    cdef defs.rpc_object_t c_cb_function(void *cookie, defs.rpc_object_t args):
        cdef Array args_array
        cdef Object result
        cdef object cb = <object>defs.rpc_function_get_arg(cookie)

        args_array = Array.__new__(Array)
        args_array.obj = args

        result = Object(cb(args_array))
        return result.obj

    def register_method(self, name, description, fn):
        defs.rpc_context_register_method_f(
            self.context,
            name,
            description,
            <void *>fn,
            <defs.rpc_function_f>Context.c_cb_function
        )

    def unregister_method(self, name):
        defs.rpc_context_unregister_method(self.context, name)

    def __dealloc__(self):
        defs.rpc_context_free(self.context)


cdef class Connection(object):
    cdef defs.rpc_connection_t connection

    def call_sync(self, method, *args):
        cdef defs.rpc_object_t rpc_result
        cdef defs.rpc_object_t *rpc_args
        cdef defs.rpc_call_t call
        cdef Object rpc_value

        rpc_args = <defs.rpc_object_t *>malloc(sizeof(defs.rpc_object_t) * len(args))
        for idx, arg in enumerate(args):
            rpc_value = Object(arg)
            rpc_args[idx] = rpc_value.obj

        call = defs.rpc_connection_call(self.connection, method, rpc_args[0])
        defs.rpc_call_wait(call)
        rpc_result = defs.rpc_call_result(call)

        defs.rpc_retain(rpc_result)

        rpc_value = Object.__new__(Object)
        rpc_value.obj = rpc_result

        free(rpc_args)
        return rpc_value.value()


    def call_async(self, method, callback, *args):
        pass

    def emit_event(self, name, data):
        pass


cdef class Client(Connection):
    cdef defs.rpc_client_t client

    def connect(self, uri):
        self.client = defs.rpc_client_create(uri, 0)
        self.connection = defs.rpc_client_get_connection(self.client)

    def disconnect(self):
        defs.rpc_client_close(self.client)


cdef class Server(Context):
    cdef defs.rpc_server_t server

    def __init__(self, uri):
        super(Context, self).__init__()
        self.server = defs.rpc_server_create(uri, self.context)
