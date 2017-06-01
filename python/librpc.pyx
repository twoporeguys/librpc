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

import sys
import enum
import errno
import types
import traceback
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


class CallStatus(enum.IntEnum):
    IN_PROGRESS = defs.RPC_CALL_IN_PROGRESS,
    MORE_AVAILABLE = defs.RPC_CALL_MORE_AVAILABLE,
    DONE = defs.RPC_CALL_DONE,
    ERROR = defs.RPC_CALL_ERROR


class LibException(Exception):
    def __init__(self, code=None, message=None, extra=None, stacktrace=None, obj=None):
        if obj:
            self.code = obj['code']
            self.message = obj['message']
            self.extra = obj.get('extra')
            return

        self.code = code
        self.message = message
        self.extra = extra

        tb = sys.exc_info()[2]
        if stacktrace is None and tb:
            stacktrace = tb

        if isinstance(stacktrace, types.TracebackType):
            def serialize_traceback(tb):
                iter_tb = tb if isinstance(tb, (list, tuple)) else traceback.extract_tb(tb)
                return [
                    {
                        'filename': f[0],
                        'lineno': f[1],
                        'method': f[2],
                        'code': f[3]
                    }
                    for f in iter_tb
                ]

            stacktrace = serialize_traceback(stacktrace)

        self.stacktrace = stacktrace

    def __str__(self):
        return "Code {0}: {1} {2}".format(
            self.code,
            self.message,
            self.extra if self.extra else ''
        )


class RpcException(LibException):
    pass


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

            return

        raise LibException(errno.EINVAL, f"Cannot create RPC object - unknown value type: {type(value)}")

    def __repr__(self):
        byte_descr = defs.rpc_copy_description(self.obj)
        return byte_descr.decode('utf-8')

    def __dealloc__(self):
        if self.obj != NULL:
            defs.rpc_release(self.obj)

    property value:
        def __get__(self):
            cdef Array array
            cdef Dictionary dictionary
            cdef const char *c_string = NULL
            cdef const uint8_t *c_bytes = NULL
            cdef size_t c_len = 0

            if self.type == ObjectType.NIL:
                return None

            if self.type == ObjectType.BOOL:
                return defs.rpc_bool_get_value(self.obj)

            if self.type == ObjectType.INT64:
                return defs.rpc_int64_get_value(self.obj)

            if self.type == ObjectType.UINT64:
                return defs.rpc_uint64_get_value(self.obj)

            if self.type == ObjectType.FD:
                return defs.rpc_fd_get_value(self.obj)

            if self.type == ObjectType.STRING:
                c_string = defs.rpc_string_get_string_ptr(self.obj)
                c_len = defs.rpc_string_get_length(self.obj)

                return c_string[:c_len].decode('UTF-8')

            if self.type == ObjectType.DOUBLE:
                return defs.rpc_double_get_value(self.obj)

            if self.type == ObjectType.DATE:
                return datetime.datetime.utcfromtimestamp(defs.rpc_date_get_value(self.obj))

            if self.type == ObjectType.BINARY:
                c_bytes = <uint8_t *>defs.rpc_data_get_bytes_ptr(self.obj)
                c_len = defs.rpc_data_get_length(self.obj)

                return <bytes>c_bytes[:c_len]

            if self.type == ObjectType.ARRAY:
                array = Array.__new__(Array)
                array.obj = self.obj
                defs.rpc_retain(array.obj)
                return array

            if self.type == ObjectType.DICTIONARY:
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
            raise LibException(errno.EINVAL, f"Cannot initialize Array RPC object from {type(value)} type")

        super(Array, self).__init__(value, force_type)

    @staticmethod
    cdef bint c_applier(void *arg, size_t index, defs.rpc_object_t value) with gil:
        cdef object cb = <object>arg
        cdef Object py_value

        py_value = Object.__new__(Object)
        py_value.obj = value

        defs.rpc_retain(py_value.obj)

        return <bint>cb(index, py_value)

    def __applier(self, applier_f):
        defs.rpc_array_apply_f(self.obj, <void *>applier_f, <defs.rpc_array_applier_f>Array.c_applier)

    def append(self, value):
        cdef Object rpc_value

        if isinstance(value, Object):
            rpc_value = value
        else:
            rpc_value = Object(value)

        defs.rpc_array_append_value(self.obj, rpc_value.obj)

    def extend(self, array):
        cdef Object rpc_value
        cdef Array rpc_array

        if isinstance(array, Array):
            rpc_array = array
        elif isinstance(array, list):
            rpc_array = Array(array)
        else:
            raise LibException(errno.EINVAL, "Array can be extended with only with list or another Array")

        for value in rpc_array:
            rpc_value = value
            defs.rpc_array_append_value(self.obj, rpc_value.obj)

    def clear(self):
        defs.rpc_release(self.obj)
        self.obj = defs.rpc_array_create()

    def copy(self):
        cdef Array copy

        copy = Array.__new__(Array)
        with nogil:
            copy.obj = defs.rpc_copy(self.obj)

        return copy

    def count(self, value):
        cdef Object v1
        cdef Object v2
        count = 0

        if isinstance(value, Object):
            v1 = value
        else:
            v1 = Object(value)

        def count_items(idx, v):
            nonlocal v2
            nonlocal count
            v2 = v

            if defs.rpc_equal(v1.obj, v2.obj):
                count += 1

            return True

        self.__applier(count_items)

        return count

    def index(self, value):
        cdef Object v1
        cdef Object v2
        index = None

        if isinstance(value, Object):
            v1 = value
        else:
            v1 = Object(value)

        def find_index(idx, v):
            nonlocal v2
            nonlocal index
            v2 = v

            if defs.rpc_equal(v1.obj, v2.obj):
                index = idx
                return False

            return True

        self.__applier(find_index)

        if index is None:
            raise LibException(errno.EINVAL, f'{value} is not in list')

        return index

    def insert(self, index, value):
        cdef Object rpc_value

        if isinstance(value, Object):
            rpc_value = value
        else:
            rpc_value = Object(value)

        defs.rpc_array_set_value(self.obj, index, rpc_value.obj)

    def pop(self, index=None):
        if index is None:
            index = self.__len__() - 1

        val = self.__getitem__(index)
        self.__delitem__(index)
        return val

    def remove(self, value):
        idx = self.index(value)
        self.__delitem__(idx)

    def __contains__(self, value):
        try:
            self.index(value)
            return True
        except ValueError:
            return False

    def __delitem__(self, index):
        defs.rpc_array_remove_index(self.obj, index)

    def __iter__(self):
        result = []
        def collect(idx, v):
            result.append(v)
            return True

        self.__applier(collect)

        for v in result:
            yield v

    def __len__(self):
        return defs.rpc_array_get_count(self.obj)

    def __sizeof__(self):
        return defs.rpc_array_get_count(self.obj)

    def __getitem__(self, index):
        cdef Object rpc_value

        rpc_value = Object.__new__(Object)
        rpc_value.obj = defs.rpc_array_get_value(self.obj, index)
        if rpc_value.obj == NULL:
            raise LibException(errno.ERANGE, 'Array index out of range')

        defs.rpc_retain(rpc_value.obj)

        return rpc_value

    def __setitem__(self, index, value):
        cdef Object rpc_value

        rpc_value = Object(value)
        defs.rpc_array_set_value(self.obj, index, rpc_value.obj)

        defs.rpc_retain(rpc_value.obj)


cdef class Dictionary(Object):
    def __init__(self, value, force_type=None):
        if not isinstance(value, dict):
            raise LibException(errno.EINVAL, "Cannot initialize Dictionary RPC object from {type(value)} type")

        super(Dictionary, self).__init__(value, force_type)

    @staticmethod
    cdef bint c_applier(void *arg, char *key, defs.rpc_object_t value) with gil:
        cdef object cb = <object>arg
        cdef Object py_value

        py_value = Object.__new__(Object)
        py_value.obj = value

        defs.rpc_retain(py_value.obj)

        return <bint>cb(key.decode('utf-8'), py_value)

    def __applier(self, applier_f):
        defs.rpc_dictionary_apply_f(self.obj, <void *>applier_f, <defs.rpc_dictionary_applier_f>Dictionary.c_applier)

    def clear(self):
        with nogil:
            defs.rpc_release(self.obj)
            self.obj = defs.rpc_dictionary_create()

    def copy(self):
        cdef Dictionary copy

        copy = Dictionary.__new__(Dictionary)
        with nogil:
            copy.obj = defs.rpc_copy(self.obj)

        return copy

    def get(self, k, d=None):
        try:
            return self.__getitem__(k)
        except KeyError:
            return d

    def items(self):
        result = []
        def collect(k, v):
            result.append((k, v))
            return True

        self.__applier(collect)
        return result

    def keys(self):
        result = []
        def collect(k, v):
            result.append(k)
            return True

        self.__applier(collect)
        return result

    def pop(self, k, d=None):
        try:
            val = self.__getitem__(k)
            self.__delitem__(k)
            return val
        except KeyError:
            return d

    def setdefault(self, k, d=None):
        try:
            return self.__getitem__(k)
        except KeyError:
            self.__setitem__(k, d)
            return d

    def update(self, value):
        cdef Dictionary py_dict
        cdef Object py_value
        equal = False

        if isinstance(value, Dictionary):
            py_dict = value
        elif isinstance(value, dict):
            py_dict = Dictionary(value)
        else:
            raise LibException(errno.EINVAL, "Dictionary can be updated only with dict or other Dictionary object")

        for k, v in py_dict.items():
            py_value = v
            byte_key = k.encode('utf-8')
            defs.rpc_dictionary_set_value(self.obj, byte_key, py_value.obj)

    def values(self):
        result = []
        def collect(k, v):
            result.append(v)
            return True

        self.__applier(collect)
        return result

    def __contains__(self, value):
        cdef Object v1
        cdef Object v2
        equal = False

        if isinstance(value, Object):
            v1 = value
        else:
            v1 = Object(value)

        def compare(k, v):
            nonlocal v2
            nonlocal equal
            v2 = v

            if defs.rpc_equal(v1.obj, v2.obj):
                equal = True
                return False
            return True

        self.__applier(compare)
        return equal


    def __delitem__(self, key):
        bytes_key = key.encode('utf-8')
        defs.rpc_dictionary_remove_key(self.obj, bytes_key)

    def __iter__(self):
        keys = self.keys()
        for k in keys:
            yield k

    def __len__(self):
        return defs.rpc_dictionary_get_count(self.obj)

    def __sizeof__(self):
        return defs.rpc_dictionary_get_count(self.obj)

    def __getitem__(self, key):
        cdef Object rpc_value
        byte_key = key.encode('utf-8')

        rpc_value = Object.__new__(Object)
        rpc_value.obj = defs.rpc_dictionary_get_value(self.obj, byte_key)
        if rpc_value.obj == NULL:
            raise LibException(errno.EINVAL, f'Key {key} does not exist')

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
        defs.PyEval_InitThreads()
        self.context = defs.rpc_context_create()

    @staticmethod
    cdef defs.rpc_object_t c_cb_function(void *cookie, defs.rpc_object_t args) with gil:
        cdef Array args_array
        cdef Object rpc_obj
        cdef object cb = <object>defs.rpc_function_get_arg(cookie)
        cdef int ret

        args_array = Array.__new__(Array)
        args_array.obj = args

        output = cb(*[a for a in args_array])
        if isinstance(output, types.GeneratorType):
            for chunk in output:
                rpc_obj = Object(chunk)
                defs.rpc_retain(rpc_obj.obj)

                with nogil:
                    ret = defs.rpc_function_yield(cookie, rpc_obj.obj)
                
                if ret:
                    break

            return defs.rpc_null_create()

        rpc_obj = Object(output)
        defs.rpc_retain(rpc_obj.obj)
        return rpc_obj.obj

    def register_method(self, name, description, fn):
        defs.rpc_context_register_func(
            self.context,
            name.encode('utf-8'),
            description.encode('utf-8'),
            <void *>fn,
            <defs.rpc_function_f>Context.c_cb_function
        )

    def unregister_method(self, name):
        defs.rpc_context_unregister_method(self.context, name)

    def __dealloc__(self):
        defs.rpc_context_free(self.context)


cdef class Connection(object):
    cdef defs.rpc_connection_t connection
    cdef object ev_handlers

    def __init__(self):
        defs.PyEval_InitThreads()
        self.ev_handlers = {}

    @staticmethod
    cdef void c_ev_handler(const char *name, defs.rpc_object_t args, void *arg) with gil:
        cdef Object event_args
        cdef object ev_handlers = <object>arg

        event_args = Object.__new__(Object)
        event_args.obj = args
        defs.rpc_retain(args)

        ev_handlers[name.decode('utf-8')](event_args)

    def call_sync(self, method, *args):
        cdef defs.rpc_object_t rpc_result
        cdef Array rpc_args
        cdef Dictionary error
        cdef defs.rpc_call_t call
        cdef Object rpc_value
        cdef defs.rpc_call_status_t call_status

        rpc_args = Array(list(args))

        call = defs.rpc_connection_call(self.connection, method.encode('utf-8'), rpc_args.obj)
        if call == NULL:
            raise_internal_excp(rpc=True)

        defs.rpc_call_wait(call)
        call_status = <defs.rpc_call_status_t>defs.rpc_call_status(call)

        def get_chunk():
            nonlocal rpc_result
            nonlocal rpc_value

            rpc_result = defs.rpc_call_result(call)
            defs.rpc_retain(rpc_result)

            rpc_value = Object.__new__(Object)
            rpc_value.obj = rpc_result
            return rpc_value

        def iter_chunk():
            nonlocal call_status

            while call_status == CallStatus.MORE_AVAILABLE:
                yield get_chunk()
                defs.rpc_call_continue(call, True)
                call_status = <defs.rpc_call_status_t>defs.rpc_call_status(call)

        if call_status == CallStatus.ERROR:
            rpc_value = get_chunk()
            defs.rpc_retain(rpc_value.obj)
            error = Dictionary.__new__(Dictionary)
            error.obj = rpc_value.obj
            raise RpcException(error['code'].value, error['message'].value)

        if call_status == CallStatus.DONE:
            return get_chunk()

        return iter_chunk()

    def call_async(self, method, callback, *args):
        pass

    def emit_event(self, name, data):
        pass

    def register_event_handler(self, name, fn):
        byte_name = name.encode('utf-8')
        self.ev_handlers[name] = fn
        defs.rpc_connection_register_event_handler_f(
            self.connection,
            byte_name,
            <defs.rpc_handler_f>Connection.c_ev_handler,
            <void *>self.ev_handlers
        )

cdef class Client(Connection):
    cdef defs.rpc_client_t client
    cdef object uri

    def __init__(self):
        super(Client, self).__init__()

    def connect(self, uri):
        self.uri = uri.encode('utf-8')
        self.client = defs.rpc_client_create(self.uri, 0)
        if self.client == NULL:
            raise_internal_excp(rpc=False)

        self.connection = defs.rpc_client_get_connection(self.client)

    def disconnect(self):
        defs.rpc_client_close(self.client)


cdef class Server(object):
    cdef defs.rpc_server_t server
    cdef Context context
    cdef object uri

    def __init__(self, uri, context):
        self.uri = uri.encode('utf-8')
        self.context = context
        self.server = defs.rpc_server_create(self.uri, self.context.context)

    def broadcast_event(self, name, args):
        cdef Object rpc_args
        byte_name = name.encode('utf-8')
        if isinstance(args, Object):
            rpc_args = args
        else:
            rpc_args = Object(args)
            defs.rpc_retain(rpc_args.obj)

        defs.rpc_server_broadcast_event(self.server, byte_name, rpc_args.obj)

    def close(self):
        defs.rpc_server_close(self.server)


cdef raise_internal_excp(rpc=False):
    cdef defs.rpc_error_t error

    excp = LibException
    if rpc:
        excp = RpcException

    error = defs.rpc_get_last_error()
    if error != NULL:
        try:
            raise excp(error.code, error.message.decode('utf-8'))
        finally:
            free(error)
