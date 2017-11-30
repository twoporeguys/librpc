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

import os
import sys
import enum
import errno
import types
import inspect
import traceback
import datetime
from librpc cimport *
from libc.string cimport strdup
from libc.stdint cimport *
from libc.stdlib cimport malloc, free


cdef extern from "Python.h" nogil:
    void PyEval_InitThreads()


class ErrorCode(enum.IntEnum):
    INVALID_RESPONSE = RPC_INVALID_RESPONSE
    CONNECTION_TIMEOUT = RPC_CONNECTION_TIMEOUT
    CONNECTION_CLOSED = RPC_CONNECTION_CLOSED
    CALL_TIMEOUT = RPC_CALL_TIMEOUT
    SPURIOUS_RESPONSE = RPC_SPURIOUS_RESPONSE
    LOGOUT = RPC_LOGOUT
    OTHER = RPC_OTHER


class ObjectType(enum.IntEnum):
    NIL = RPC_TYPE_NULL
    BOOL = RPC_TYPE_BOOL
    UINT64 = RPC_TYPE_UINT64
    INT64 = RPC_TYPE_INT64
    DOUBLE = RPC_TYPE_DOUBLE
    DATE = RPC_TYPE_DATE
    STRING = RPC_TYPE_STRING
    BINARY = RPC_TYPE_BINARY
    FD = RPC_TYPE_FD
    ERROR = RPC_TYPE_ERROR
    DICTIONARY = RPC_TYPE_DICTIONARY
    ARRAY = RPC_TYPE_ARRAY


class CallStatus(enum.IntEnum):
    IN_PROGRESS = RPC_CALL_IN_PROGRESS,
    MORE_AVAILABLE = RPC_CALL_MORE_AVAILABLE,
    DONE = RPC_CALL_DONE,
    ERROR = RPC_CALL_ERROR
    ABORTED = RPC_CALL_ABORTED


class BusEvent(enum.IntEnum):
    ATTACHED = RPC_BUS_ATTACHED
    DETACHED = RPC_BUS_DETACHED


class TypeClass(enum.IntEnum):
    STRUCT = RPC_TYPING_STRUCT
    UNION = RPC_TYPING_UNION
    ENUM = RPC_TYPING_ENUM
    TYPEDEF = RPC_TYPING_TYPEDEF
    BUILTIN = RPC_TYPING_BUILTIN


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
        self.obj = obj

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
        return "[{0}] {1}".format(os.strerror(self.code), self.message)


class RpcException(LibException):
    pass


cdef class Object(object):
    def __init__(self, value, force_type=None):
        if value is None or force_type == ObjectType.NIL:
            self.obj = rpc_null_create()
            return

        if isinstance(value, bool) or force_type == ObjectType.BOOL:
            self.obj = rpc_bool_create(value)
            return

        if isinstance(value, int) or force_type == ObjectType.INT64:
            self.obj = rpc_int64_create(value)
            return

        if isinstance(value, int) and force_type == ObjectType.UINT64:
            self.obj = rpc_uint64_create(value)
            return

        if isinstance(value, int) and force_type == ObjectType.FD:
            self.obj = rpc_fd_create(value)
            return

        if isinstance(value, str) or force_type == ObjectType.STRING:
            byte_str = value.encode('utf-8')
            self.obj = rpc_string_create(byte_str)
            return

        if isinstance(value, float) or force_type == ObjectType.DOUBLE:
            self.obj = rpc_double_create(value)
            return

        if isinstance(value, datetime.datetime) or force_type == ObjectType.DATE:
            self.obj = rpc_date_create(int(value.timestamp()))
            return

        if isinstance(value, (bytearray, bytes)) or force_type == ObjectType.BINARY:
            self.ref = value
            self.obj = rpc_data_create(<char *>value, <size_t>len(value), NULL)
            return

        if isinstance(value, (RpcException, LibException)):
            extra = Object(value.extra)
            stack = Object(value.stacktrace)
            self.obj = rpc_error_create_with_stack(value.code, value.message.encode('utf-8'), extra.obj, stack.obj)
            return

        if isinstance(value, list) or force_type == ObjectType.ARRAY:
            self.obj = rpc_array_create()

            for v in value:
                child = Object(v)
                rpc_array_append_value(self.obj, child.obj)

            return

        if isinstance(value, dict) or force_type == ObjectType.DICTIONARY:
            self.obj = rpc_dictionary_create()

            for k, v in value.items():
                byte_k = k.encode('utf-8')
                child = Object(v)
                rpc_dictionary_set_value(self.obj, byte_k, child.obj)

            return

        if isinstance(value, Object):
            self.obj = rpc_copy((<Object>value).obj)
            return

        raise LibException(errno.EINVAL, "Unknown value type: {0}".format(type(value)))

    def __repr__(self):
        byte_descr = rpc_copy_description(self.obj)
        return byte_descr.decode('utf-8')

    def __str__(self):
        return str(self.value)

    def __bool__(self):
        return bool(self.value)

    def __int__(self):
        if self.type in (ObjectType.BOOL, ObjectType.UINT64, ObjectType.INT64, ObjectType.FD, ObjectType.DOUBLE):
            return int(self.value)

        raise TypeError('int() argument must be a number, bool or fd')

    def __float__(self):
        if self.type in (ObjectType.BOOL, ObjectType.UINT64, ObjectType.INT64, ObjectType.DOUBLE):
            return int(self.value)

        raise TypeError('float() argument must be a number or bool')

    def __dealloc__(self):
        if self.obj != <rpc_object_t>NULL:
            rpc_release(self.obj)

    @staticmethod
    cdef Object init_from_ptr(rpc_object_t ptr):
        cdef Object ret

        if ptr == <rpc_object_t>NULL:
            return None

        ret = Object.__new__(Object)
        ret.obj = ptr
        rpc_retain(ret.obj)
        return ret

    def unpack(self):
        if self.type == ObjectType.DICTIONARY:
            return {k: v.unpack() for k, v in self.value.items()}

        if self.type == ObjectType.ARRAY:
            return [i.unpack() for i in self.value]

        if self.type == ObjectType.FD:
            return self

        return self.value

    property value:
        def __get__(self):
            cdef Array array
            cdef Dictionary dictionary
            cdef Object extra
            cdef Object stack
            cdef const char *c_string = NULL
            cdef const uint8_t *c_bytes = NULL
            cdef size_t c_len = 0

            if self.type == ObjectType.NIL:
                return None

            if self.type == ObjectType.BOOL:
                return rpc_bool_get_value(self.obj)

            if self.type == ObjectType.INT64:
                return rpc_int64_get_value(self.obj)

            if self.type == ObjectType.UINT64:
                return rpc_uint64_get_value(self.obj)

            if self.type == ObjectType.FD:
                return rpc_fd_get_value(self.obj)

            if self.type == ObjectType.STRING:
                c_string = rpc_string_get_string_ptr(self.obj)
                c_len = rpc_string_get_length(self.obj)
                return c_string[:c_len].decode('utf-8')

            if self.type == ObjectType.DOUBLE:
                return rpc_double_get_value(self.obj)

            if self.type == ObjectType.DATE:
                return datetime.datetime.utcfromtimestamp(rpc_date_get_value(self.obj))

            if self.type == ObjectType.BINARY:
                c_bytes = <uint8_t *>rpc_data_get_bytes_ptr(self.obj)
                c_len = rpc_data_get_length(self.obj)
                return <bytes>c_bytes[:c_len]

            if self.type == ObjectType.ERROR:
                extra = Object.__new__(Object)
                extra.obj = rpc_error_get_extra(self.obj)
                stack = Object.__new__(Object)
                stack.obj = rpc_error_get_stack(self.obj)

                return RpcException(
                    rpc_error_get_code(self.obj),
                    rpc_error_get_message(self.obj).decode('utf-8'),
                    extra,
                    stack
                )

            if self.type == ObjectType.ARRAY:
                array = Array.__new__(Array)
                array.obj = self.obj
                rpc_retain(array.obj)
                return array

            if self.type == ObjectType.DICTIONARY:
                dictionary = Dictionary.__new__(Dictionary)
                dictionary.obj = self.obj
                rpc_retain(dictionary.obj)
                return dictionary

    property type:
        def __get__(self):
            return ObjectType(rpc_get_type(self.obj))


cdef class Array(Object):
    def __init__(self, value, force_type=None):
        if not isinstance(value, list):
            raise LibException(errno.EINVAL, "Cannot initialize array from {0} type".format(type(value)))

        super(Array, self).__init__(value, force_type)

    @staticmethod
    cdef bint c_applier(void *arg, size_t index, rpc_object_t value) with gil:
        cdef object cb = <object>arg
        cdef Object py_value

        py_value = Object.__new__(Object)
        py_value.obj = value

        rpc_retain(py_value.obj)

        return <bint>cb(index, py_value)

    def __applier(self, applier_f):
        rpc_array_apply(
            self.obj,
            RPC_ARRAY_APPLIER(
                <rpc_array_applier_f>Array.c_applier,
                <void *>applier_f,
            )
        )

    def append(self, value):
        cdef Object rpc_value

        if isinstance(value, Object):
            rpc_value = value
        else:
            rpc_value = Object(value)

        rpc_array_append_value(self.obj, rpc_value.obj)

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
            rpc_array_append_value(self.obj, rpc_value.obj)

    def clear(self):
        rpc_release(self.obj)
        self.obj = rpc_array_create()

    def copy(self):
        cdef Array copy

        copy = Array.__new__(Array)
        with nogil:
            copy.obj = rpc_copy(self.obj)

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

            if rpc_equal(v1.obj, v2.obj):
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

            if rpc_equal(v1.obj, v2.obj):
                index = idx
                return False

            return True

        self.__applier(find_index)

        if index is None:
            raise LibException(errno.EINVAL, '{} is not in list'.format(value))

        return index

    def insert(self, index, value):
        cdef Object rpc_value

        if isinstance(value, Object):
            rpc_value = value
        else:
            rpc_value = Object(value)

        rpc_array_set_value(self.obj, index, rpc_value.obj)

    def pop(self, index=None):
        if index is None:
            index = self.__len__() - 1

        val = self.__getitem__(index)
        self.__delitem__(index)
        return val

    def remove(self, value):
        idx = self.index(value)
        self.__delitem__(idx)

    def __str__(self):
        return repr(self)

    def __contains__(self, value):
        try:
            self.index(value)
            return True
        except ValueError:
            return False

    def __delitem__(self, index):
        rpc_array_remove_index(self.obj, index)

    def __iter__(self):
        result = []
        def collect(idx, v):
            result.append(v)
            return True

        self.__applier(collect)

        for v in result:
            yield v

    def __len__(self):
        return rpc_array_get_count(self.obj)

    def __sizeof__(self):
        return rpc_array_get_count(self.obj)

    def __getitem__(self, index):
        cdef Object rpc_value

        rpc_value = Object.__new__(Object)
        rpc_value.obj = rpc_array_get_value(self.obj, index)
        if rpc_value.obj == <rpc_object_t>NULL:
            raise LibException(errno.ERANGE, 'Array index out of range')

        rpc_retain(rpc_value.obj)

        return rpc_value

    def __setitem__(self, index, value):
        cdef Object rpc_value

        rpc_value = Object(value)
        rpc_array_set_value(self.obj, index, rpc_value.obj)

        rpc_retain(rpc_value.obj)

    def __bool__(self):
        return len(self) > 0



cdef class Dictionary(Object):
    def __init__(self, value, force_type=None):
        if not isinstance(value, dict):
            raise LibException(errno.EINVAL, "Cannot initialize Dictionary RPC object from {type(value)} type")

        super(Dictionary, self).__init__(value, force_type)

    @staticmethod
    cdef bint c_applier(void *arg, char *key, rpc_object_t value) with gil:
        cdef object cb = <object>arg
        cdef Object py_value

        py_value = Object.__new__(Object)
        py_value.obj = value

        rpc_retain(py_value.obj)

        return <bint>cb(key.decode('utf-8'), py_value)

    def __applier(self, applier_f):
        rpc_dictionary_apply(
            self.obj,
            RPC_DICTIONARY_APPLIER(
                <rpc_dictionary_applier_f>Dictionary.c_applier,
                <void *>applier_f
            )
        )

    def clear(self):
        with nogil:
            rpc_release(self.obj)
            self.obj = rpc_dictionary_create()

    def copy(self):
        cdef Dictionary copy

        copy = Dictionary.__new__(Dictionary)
        with nogil:
            copy.obj = rpc_copy(self.obj)

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
            rpc_dictionary_set_value(self.obj, byte_key, py_value.obj)

    def values(self):
        result = []
        def collect(k, v):
            result.append(v)
            return True

        self.__applier(collect)
        return result

    def __str__(self):
        return repr(self)

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

            if rpc_equal(v1.obj, v2.obj):
                equal = True
                return False
            return True

        self.__applier(compare)
        return equal

    def __delitem__(self, key):
        bytes_key = key.encode('utf-8')
        rpc_dictionary_remove_key(self.obj, bytes_key)

    def __iter__(self):
        keys = self.keys()
        for k in keys:
            yield k

    def __len__(self):
        return rpc_dictionary_get_count(self.obj)

    def __sizeof__(self):
        return rpc_dictionary_get_count(self.obj)

    def __getitem__(self, key):
        cdef Object rpc_value
        byte_key = key.encode('utf-8')

        rpc_value = Object.__new__(Object)
        rpc_value.obj = rpc_dictionary_get_value(self.obj, byte_key)
        if rpc_value.obj == <rpc_object_t>NULL:
            raise LibException(errno.EINVAL, 'Key {} does not exist'.format(key))

        rpc_retain(rpc_value.obj)

        return rpc_value

    def __setitem__(self, key, value):
        cdef Object rpc_value
        byte_key = key.encode('utf-8')

        rpc_value = Object(value)
        rpc_dictionary_set_value(self.obj, byte_key, rpc_value.obj)

        rpc_retain(rpc_value.obj)

    def __bool__(self):
        return len(self) > 0


cdef class Context(object):
    def __init__(self):
        PyEval_InitThreads()
        self.methods = {}
        self.instances = {}
        self.context = rpc_context_create()

    @staticmethod
    cdef Context init_from_ptr(rpc_context_t ptr):
        cdef Context ret

        ret = Context.__new__(Context)
        ret.methods = {}
        ret.instances = {}
        ret.borrowed = True
        ret.context = ptr
        return ret

    def register_instance(self, Instance instance):
        self.instances[instance.path] = instance
        if rpc_context_register_instance(self.context, instance.instance) != 0:
            raise_internal_exc()

    def register_method(self, name, fn, interface=''):
        self.methods[name] = fn
        if rpc_context_register_func(
            self.context,
            interface.encode('utf-8'),
            name.encode('utf-8'),
            <void *>fn,
            <rpc_function_f>c_cb_function
        ) != 0:
            raise_internal_exc()

    def unregister_member(self, name, interface=None):
        del self.methods[name]
        rpc_context_unregister_member(self.context, interface, name)

    def __materialized_paths_to_tree(self, lst, separator='.'):
        result = {'children': {}, 'path': []}

        def add(parent, path):
            if not path:
                return

            p = path.pop(0)
            c = parent['children'].get(p)
            if not c:
                c = {'children': {}, 'path': parent['path'] + [p], 'label': p}
                parent['children'][p] = c

            add(c, path)

        for i in lst:
            path = i.split(separator)
            add(result, path)

        return result

    def __dealloc__(self):
        if not self.borrowed:
            rpc_context_free(self.context)


cdef class Instance(object):
    @staticmethod
    cdef Instance init_from_ptr(rpc_instance_t ptr):
        pass

    property path:
        def __get__(self):
            return rpc_instance_get_path(self.instance).decode('utf-8')

    def __init__(self, path, description='Generic instance'):
        cdef rpc_interface iface

        self.methods = {}
        self.interfaces = {}
        self.instance = rpc_instance_new(
            path.encode('utf-8'),
            description.encode('utf-8'),
            <void *>self
        )

        for name, i in inspect.getmembers(self, inspect.ismethod):
            if getattr(i, '_librpc_method', None):
                name = getattr(i, '_librpc_name', name)
                interface = getattr(i, '_librpc_interface', getattr(self, '_librpc_interface', None))
                b_name = name.encode('utf-8')
                b_interface = interface.encode('utf-8')

                print('name = {0}, interface = {1}'.format(name, interface))

                if interface not in self.interfaces:
                    iface.ri_name = b_interface
                    iface.ri_description = ""
                    iface.ri_members[0].rim_name = NULL
                    if rpc_instance_register_interface(self.instance, &iface, <void *>self) != 0:
                        raise_internal_exc()

                    self.interfaces[interface] = True

                if rpc_instance_register_func(
                    self.instance,
                    b_interface,
                    b_name,
                    <void *>i,
                    <rpc_function_f>c_cb_function
                ) != 0:
                    raise_internal_exc()

    @staticmethod
    def method(interface=None, name=None):
        def wrapped(fn):
            if name:
                fn._librpc_name = name

            if interface:
                fn._librpc_interface = interface

            fn._librpc_method = True
            return fn

        return wrapped


cdef class RemoteObject(object):
    cdef object client
    cdef object path

    def __init__(self, client, path):
        self.client = client
        self.path = path

    property path:
        def __get__(self):
            return self.path

    property description:
        def __get__(self):
            pass

    property interfaces:
        def __get__(self):
            ifaces = self.client.call_sync(
                'get_interfaces',
                path=self.path,
                interface='com.twoporeguys.librpc.Introspectable',
                unpack=True
            )

            return {i: RemoteInterface(self.client, self.path, i) for i in ifaces}

    def __str__(self):
        return "<librpc.RemoteObject at '{0}'>".format(self.path)

    def __repr__(self):
        return str(self)


cdef class RemoteInterface(object):
    def __init__(self, client, path, interface=''):
        self.client = client
        self.path = path
        self.interface = interface
        self.methods = {}
        self.properties = {}

        try:
            if not self.client.call_sync(
                'interface_exists',
                interface,
                path=path,
                interface='com.twoporeguys.librpc.Introspectable'
            ):
                raise ValueError('Interface not found')
        except:
            raise

        self.__collect_methods()
        self.__collect_properties()

    def call_sync(self, name, *args):
        return self.client.call_sync(name, *args, path=self.path, interface=self.interface)

    def __collect_methods(self):
        try:
            for method in self.client.call_sync(
                'get_methods',
                self.interface,
                path=self.path,
                interface='com.twoporeguys.librpc.Introspectable',
                unpack=True
            ):
                def fn(*args):
                    return self.client.call_sync(
                        method,
                        *args,
                        path=self.path,
                        interface=self.interface
                    )

                self.methods[method] = fn
                setattr(self, method, fn)
        except:
            pass

    def __collect_properties(self):
        try:
            for prop in self.client.call_sync(
                'get_all',
                self.interface,
                path=self.path,
                interface='com.twoporeguys.librpc.Observable',
                unpack=True
            ):
                def fn(name=prop['name']):
                    return self.client.call_sync(
                        'get',
                        self.interface,
                        name,
                        path=self.path,
                        interface='com.twoporeguys.librpc.Observable'
                    )

                self.properties[prop['name']] = fn
        except:
            pass

    def __collect_events(self):
        pass

    def __getattr__(self, item):
        return self.properties[item]()

    def __str__(self):
        return "<librpc.RemoteInterface '{0}' at '{1}'>".format(
            self.interface,
            self.path
        )

    def __repr__(self):
        return str(self)


cdef class Call(object):
    @staticmethod
    cdef Call init_from_ptr(rpc_call_t ptr):
        cdef Call result

        result = Call.__new__(Call)
        result.call = ptr
        return result

    property status:
        def __get__(self):
            return CallStatus(rpc_call_status(self.call))

    property result:
        def __get__(self):
            with nogil:
                rpc_call_wait(self.call)

            return Object.init_from_ptr(rpc_call_result(self.call))

    def __dealloc__(self):
        rpc_call_free(self.call)

    def __iter__(self):
        return self

    def __next__(self):
        result = self.result

        if result is None:
            raise StopIteration()

        if result.type == ObjectType.ERROR:
            raise result.value

        self.resume()
        return result

    def abort(self):
        with nogil:
            rpc_call_abort(self.call)

    def resume(self):
        with nogil:
            rpc_call_continue(self.call, True)


cdef class Connection(object):
    def __init__(self):
        PyEval_InitThreads()
        self.error_handler = None
        self.event_handler = None
        self.unpack = False
        self.ev_handlers = []

    property error_handler:
        def __get__(self):
            return self.error_handler

        def __set__(self, fn):
            if not callable(fn):
                fn = None

            self.error_handler = fn
            rpc_connection_set_error_handler(self.connection,
                RPC_ERROR_HANDLER(
                    self.c_error_handler,
                    <void *>self.error_handler
                )
            )

    property event_handler:
        def __get__(self):
            return self.event_handler

        def __set__(self, fn):
            self.event_handler = fn

    @staticmethod
    cdef Connection init_from_ptr(rpc_connection_t ptr):
        cdef Connection ret

        ret = Connection.__new__(Connection)
        ret.borrowed = True
        ret.connection = ptr
        return ret

    @staticmethod
    cdef void c_ev_handler(void *arg, const char *path, const char *interface, const char *name, rpc_object_t args) with gil:
        cdef Object event_args
        cdef object handler = <object>arg

        event_args = Object.__new__(Object)
        event_args.obj = args
        rpc_retain(args)

        handler(event_args)

    @staticmethod
    cdef void c_error_handler(void *arg, rpc_error_code_t error, rpc_object_t args) with gil:
        cdef Object error_args = None
        cdef object handler = <object>arg

        if args != <rpc_object_t>NULL:
            error_args = Object.__new__(Object)
            error_args.obj = args
            rpc_retain(args)

        handler(ErrorCode(error), error_args)

    def call(self, const char *path, const char *interface, const char *method, *args):
        cdef Array rpc_args
        cdef Call result

        if self.connection == <rpc_connection_t>NULL:
            raise RuntimeError("Not connected")

        rpc_args = Array(list(args))
        rpc_retain(rpc_args.obj)

        with nogil:
            call = rpc_connection_call(self.connection, path, interface, method, rpc_args.obj, NULL)

        if call == <rpc_call_t>NULL:
            raise_internal_exc(rpc=True)

        result = Call.init_from_ptr(call)
        result.connection = self
        return result

    def call_sync(self, method, *args, path='/', interface=None, unpack=None):
        cdef rpc_object_t rpc_result
        cdef Array rpc_args
        cdef Dictionary error
        cdef rpc_call_t call
        cdef rpc_call_status_t call_status
        cdef const char *c_path
        cdef const char *c_interface = NULL
        cdef const char *c_method

        if self.connection == <rpc_connection_t>NULL:
            raise RuntimeError("Not connected")

        rpc_args = Array(list(args))
        b_path = path.encode('utf-8')
        c_path = b_path
        b_method = method.encode('utf-8')
        c_method = b_method

        if interface:
            b_interface = interface.encode('utf-8')
            c_interface = b_interface

        with nogil:
            call = rpc_connection_call(self.connection, c_path, c_interface, c_method, rpc_args.obj, NULL)

        if call == <rpc_call_t>NULL:
            raise_internal_exc(rpc=True)

        with nogil:
            rpc_call_wait(call)

        call_status = rpc_call_status(call)

        def get_chunk():
            nonlocal rpc_result

            rpc_result = rpc_call_result(call)
            rpc_retain(rpc_result)

            return self.do_unpack(Object.init_from_ptr(rpc_result), unpack)

        def iter_chunk():
            nonlocal call_status

            while call_status == CallStatus.MORE_AVAILABLE:
                try:
                    yield get_chunk()
                    with nogil:
                        rpc_call_continue(call, True)

                    call_status = rpc_call_status(call)
                except:
                    with nogil:
                        rpc_call_abort(call)

                    break

        if call_status == CallStatus.ERROR:
            raise get_chunk()

        if call_status == CallStatus.DONE:
            return get_chunk()

        if call_status == CallStatus.MORE_AVAILABLE:
            return iter_chunk()

        raise AssertionError('Impossible call status {0}'.format(call_status))

    def do_unpack(self, value, unpack=None):
        unpack = self.unpack if unpack is None else unpack
        return value.unpack() if unpack else value

    def call_async(self, method, callback, *args):
        pass

    def emit_event(self, name, Object data, path='/', interface=None):
        cdef const char *c_path
        cdef const char *c_name
        cdef const char *c_interface = NULL

        b_path = path.encode('utf-8')
        c_path = b_path
        b_name = name.encode('utf-8')
        c_name = b_name

        if interface:
            b_interface = interface.encode('utf-8')
            c_interface = b_interface

        with nogil:
            rpc_connection_send_event(self.connection, c_path, c_interface, c_name, data.obj)

    def register_event_handler(self, name, fn):
        if self.connection == <rpc_connection_t>NULL:
            raise RuntimeError("Not connected")

        byte_name = name.encode('utf-8')
        self.ev_handlers.append(fn)
        rpc_connection_register_event_handler(
            self.connection,
            byte_name,
            RPC_HANDLER(
                <rpc_handler_f>Connection.c_ev_handler,
                <void *>fn
            )
        )

cdef class Client(Connection):
    cdef rpc_client_t client
    cdef object uri

    def __init__(self):
        super(Client, self).__init__()
        self.uri = None
        self.client = <rpc_client_t>NULL
        self.connection = <rpc_connection_t>NULL

    def __dealloc__(self):
        if self.client != <rpc_client_t>NULL:
            rpc_client_close(self.client)

    def connect(self, uri, Object params=None):
        cdef char* c_uri
        cdef rpc_object_t rawparams

        self.uri = c_uri = uri.encode('utf-8')
        rawparams = params.obj if params else <rpc_object_t>NULL
        with nogil:
            self.client = rpc_client_create(c_uri, rawparams)

        if self.client == <rpc_client_t>NULL:
            raise_internal_exc(rpc=False)

        self.connection = rpc_client_get_connection(self.client)

    def disconnect(self):
        if self.client == <rpc_client_t>NULL:
            raise RuntimeError("Not connected")

        rpc_client_close(self.client)
        self.client = <rpc_client_t>NULL

    property instances:
        def __get__(self):
            objects = self.call_sync('get_instances', interface='com.twoporeguys.librpc.Discoverable', path='/', unpack=True)
            return {o['path']: RemoteObject(self, o['path']) for o in objects}


cdef class Server(object):
    cdef rpc_server_t server
    cdef Context context
    cdef object uri

    def __init__(self, uri, context):
        if not uri:
            raise RuntimeError('URI cannot be empty')

        self.uri = uri.encode('utf-8')
        self.context = context
        self.server = rpc_server_create(self.uri, self.context.context)

    def broadcast_event(self, name, args, path='/', interface=None):
        cdef Object rpc_args
        cdef const char *c_name
        cdef const char *c_path
        cdef const char *c_interface = NULL

        b_name = name.encode('utf-8')
        c_name = b_name
        b_path = path.encode('utf-8')
        c_path = b_path

        if interface:
            b_interface = interface.encode('utf-8')
            c_interface = b_interface

        if isinstance(args, Object):
            rpc_args = args
        else:
            rpc_args = Object(args)
            rpc_retain(rpc_args.obj)

        with nogil:
            rpc_server_broadcast_event(self.server, c_path, c_interface, c_name, rpc_args.obj)

    def close(self):
        rpc_server_close(self.server)


cdef class BusNode(object):
    cdef readonly object name
    cdef readonly object description
    cdef readonly object serial
    cdef readonly uint32_t address


cdef class Bus(object):
    def __init__(self):
        cdef int ret

        PyEval_InitThreads()
        with nogil:
            ret = rpc_bus_open()

        if ret != 0:
            raise_internal_exc()

    def __dealloc__(self):
        with nogil:
            rpc_bus_close()

    def ping(self, name):
        cdef const char *c_name
        cdef int ret

        b_name = name.encode('utf-8')
        c_name = b_name

        with nogil:
            ret = rpc_bus_ping(c_name)

        if ret < 0:
            raise_internal_exc()

    def enumerate(self):
        cdef BusNode node
        cdef rpc_bus_node *result
        cdef int ret

        with nogil:
            ret = rpc_bus_enumerate(&result)

        if ret < 0:
            raise_internal_exc()

        try:
            for i in range(0, ret):
                node = BusNode.__new__(BusNode)
                node.name = result[i].rbn_name.decode('utf-8')
                node.description = result[i].rbn_description.decode('utf-8')
                node.serial = result[i].rbn_serial.decode('utf-8')
                node.address = result[i].rbn_address
                yield node
        finally:
            rpc_bus_free_result(result)

    @staticmethod
    cdef void c_ev_handler(void *arg, rpc_bus_event_t ev, rpc_bus_node *bn) with gil:
        cdef object fn = <object>arg
        cdef BusNode node

        node = BusNode.__new__(BusNode)
        node.address = bn.rbn_address

        if bn.rbn_name:
            node.name = bn.rbn_name.decode('utf-8')

        if bn.rbn_description:
            node.description = bn.rbn_description.decode('utf-8')

        if bn.rbn_serial:
            node.serial = bn.rbn_serial.decode('utf-8')

        fn(BusEvent(ev), node)

    property event_handler:
        def __set__(self, value):
            if not value:
                rpc_bus_unregister_event_handler()
                self.event_fn = None
                return

            self.event_fn = value
            rpc_bus_register_event_handler(RPC_BUS_EVENT_HANDLER(
                self.c_ev_handler,
                <void *>self.event_fn
            ))


cdef class Serializer(object):
    cdef const char *type

    def __init__(self, type):
        if not rpc_serializer_exists(type.encode('ascii')):
            raise LibException(errno.ENOENT, 'Unknown serializer')

        self.type = strdup(type.encode('ascii'))

    def __dealloc__(self):
        free(<void *>self.type)

    def loads(self, bytes blob):
        cdef Object ret
        cdef char *buf = blob
        cdef int length = len(blob)

        ret = Object.__new__(Object)

        with nogil:
            ret.obj = rpc_serializer_load(self.type, buf, length)

        if ret.obj == <rpc_object_t>NULL:
            raise_internal_exc()

        return ret

    def dumps(self, Object obj):
        cdef void *frame
        cdef size_t len
        cdef int ret

        with nogil:
            ret = rpc_serializer_dump(self.type, obj.obj, &frame, &len)

        if ret != 0:
            raise_internal_exc()

        return <bytes>(<char *>frame)[:len]


cdef class Typing(object):
    def __init__(self):
        with nogil:
            rpct_init()

    @staticmethod
    cdef bint c_iter(void *arg, rpct_type_t val):
        cdef Type rpctype
        cdef object container

        container = <object>arg
        rpctype = Type.__new__(Type)
        rpctype.rpctype = val
        container.append(rpctype)
        return True

    def load_type(self, decl, type):
        pass

    def load_types(self, path):
        if rpct_load_types(path.encode('utf-8')) != 0:
            raise_internal_exc()

    def serialize(self, Object obj):
        cdef rpc_object_t result

        result = rpct_serialize(obj.obj)
        return Object.init_from_ptr(result)

    def deserialize(self, Object obj):
        cdef rpc_object_t result

        result = rpct_deserialize(obj.obj)
        return Object.init_from_ptr(result)

    property types:
        def __get__(self):
            ret = []
            rpct_types_apply(RPCT_TYPE_APPLIER(self.c_iter, <void *>ret))
            return ret

    property functions:
        def __get__(self):
            pass

    property files:
        def __get__(self):
            pass

    property realm:
        def __get__(self):
            return rpct_get_realm().decode('utf-8')

        def __set__(self, value):
            rpct_set_realm(value.encode('utf-8'))


cdef class TypeInstance(object):
    cdef rpct_typei_t rpctypei

    def __init__(self, decl):
        self.rpctypei = rpct_new_typei(decl.encode('utf-8'))
        if self.rpctypei == <rpct_typei_t>NULL:
            raise ValueError('Invalid type specifier')

    def __str__(self):
        return '<librpc.TypeInstance "{0}">'.format(self.canonical)

    def __repr__(self):
        return str(self)

    @staticmethod
    cdef TypeInstance init_from_ptr(rpct_typei_t typei):
        cdef TypeInstance ret

        if typei == <rpct_typei_t>NULL:
            return None

        ret = TypeInstance.__new__(TypeInstance)
        ret.rpctypei = typei
        return ret

    property factory:
        def __get__(self):
            return type(self.canonical, (BaseTypingObject,), {
                'typei': self
            })

    property type:
        def __get__(self):
            cdef Type typ

            typ = Type.__new__(Type)
            typ.rpctype = rpct_typei_get_type(self.rpctypei)
            return typ

    property canonical:
        def __get__(self):
            return rpct_typei_get_canonical_form(self.rpctypei).decode('utf-8')

    property generic_variables:
        def __get__(self):
            cdef TypeInstance typei

            result = []
            count = rpct_type_get_generic_vars_count(rpct_typei_get_type(self.rpctypei))
            for i in range(count):
                typei = TypeInstance.__new__(TypeInstance)
                typei.rpctypei = rpct_typei_get_generic_var(self.rpctypei, i)
                result.append(typei)

            return result

    def validate(self, Object obj):
        cdef rpc_object_t errors
        cdef bint valid

        valid = rpct_validate(self.rpctypei, obj.obj, &errors)
        return valid, Object.init_from_ptr(errors)


cdef class Type(object):
    cdef rpct_type_t rpctype

    @staticmethod
    cdef bint c_iter(void *arg, rpct_member_t val):
        cdef Member member
        cdef object container

        container = <object>arg
        member = Member.__new__(Member)
        member.rpcmem = val
        container.append(member)
        return True

    def __str__(self):
        return "<librpc.Type '{0}'>".format(self.name)

    def __repr__(self):
        return str(self)

    property name:
        def __get__(self):
            return rpct_type_get_name(self.rpctype).decode('utf-8')

    property module:
        def __get__(self):
            return rpct_type_get_module(self.rpctype).decode('utf-8')

    property description:
        def __get__(self):
            return rpct_type_get_description(self.rpctype).decode('utf-8')

    property generic:
        def __get__(self):
            return rpct_type_get_generic_vars_count(self.rpctype) > 0

    property generic_variables:
        def __get__(self):
            ret = []
            count = rpct_type_get_generic_vars_count(self.rpctype)

            for i in range(count):
                ret.append(rpct_type_get_generic_var(self.rpctype, i).decode('utf-8'))

            return ret

    property clazz:
        def __get__(self):
            return TypeClass(rpct_type_get_class(self.rpctype))

    property members:
        def __get__(self):
            ret = []
            rpct_members_apply(self.rpctype, RPCT_MEMBER_APPLIER(self.c_iter, <void *>ret))
            return ret

    property definition:
        def __get__(self):
            return TypeInstance.init_from_ptr(rpct_type_get_definition(self.rpctype))

    property is_struct:
        def __get__(self):
            return self.clazz == TypeClass.STRUCT

    property is_union:
        def __get__(self):
            return self.clazz == TypeClass.UNION

    property is_enum:
        def __get__(self):
            return self.clazz == TypeClass.ENUM

    property is_typedef:
        def __get__(self):
            return self.clazz == TypeClass.TYPEDEF

    property is_builtin:
        def __get__(self):
            return self.clazz == TypeClass.BUILTIN


cdef class Member(object):
    cdef rpct_member_t rpcmem

    property type:
        def __get__(self):
            return TypeInstance.init_from_ptr(rpct_member_get_typei(self.rpcmem))

    property name:
        def __get__(self):
            return rpct_member_get_name(self.rpcmem).decode('utf-8')

    property description:
        def __get__(self):
            return rpct_member_get_description(self.rpcmem).decode('utf-8')


cdef class Function(object):
    cdef rpct_function_t c_func

    property name:
        def __get__(self):
            return rpct_function_get_name(self.c_func).decode('utf-8')

    property description:
        def __get__(self):
            return rpct_function_get_description(self.c_func).decode('utf-8')

    property return_type:
        def __get__(self):
            pass

    property arguments:
        def __get__(self):
            pass


cdef class FunctionArgument(object):
    cdef rpct_argument_t c_arg

    property description:
        def __get__(self):
            pass

    property type:
        def __get__(self):
            pass


cdef class StructUnionMember(Member):
    def specialize(self, TypeInstance typei):
        return TypeInstance.init_from_ptr(rpct_typei_get_member_type(typei.rpctypei, self.rpcmem))


cdef class BaseTypingObject(object):
    typei = None
    cdef rpc_object_t obj

    def __init__(self, Object value):
        cdef TypeInstance typei

        typei = <TypeInstance>self.__class__.typei
        self.obj = rpct_newi(typei.rpctypei, value.obj)

    property type:
        def __get__(self):
            cdef TypeInstance typei

            typei = TypeInstance.__new__(TypeInstance)
            typei.rpctypei = rpct_get_typei(self.obj)
            return typei

    property value:
        def __get__(self):
            return Object.init_from_ptr(self.obj)


cdef class BaseStruct(BaseTypingObject):
    def __getattr__(self, item):
        pass

    def __setattr__(self, key, value):
        pass


cdef class BaseUnion(BaseTypingObject):
    property branch:
        def __get__(self):
            pass

    property value:
        def __get__(self):
            pass


cdef class BaseEnum(BaseTypingObject):
    property value:
        def __get__(self):
            pass


cdef rpc_object_t c_cb_function(void *cookie, rpc_object_t args) with gil:
    cdef Array args_array
    cdef Object rpc_obj
    cdef object cb = <object>rpc_function_get_arg(cookie)
    cdef int ret

    args_array = Array.__new__(Array)
    args_array.obj = args

    try:
        output = cb(*[a for a in args_array])
        if isinstance(output, types.GeneratorType):
            for chunk in output:
                rpc_obj = Object(chunk)
                rpc_retain(rpc_obj.obj)

                with nogil:
                    ret = rpc_function_yield(cookie, rpc_obj.obj)

                if ret:
                    break

            return <rpc_object_t>NULL
    except Exception as e:
        if not isinstance(e, RpcException):
            e = RpcException(errno.EFAULT, str(e))

        rpc_obj = Object(e)
        rpc_function_error_ex(cookie, rpc_obj.obj)
        return <rpc_object_t>NULL

    rpc_obj = Object(output)
    rpc_retain(rpc_obj.obj)
    return rpc_obj.obj


cdef raise_internal_exc(rpc=False):
    cdef rpc_object_t error

    error = rpc_get_last_error()
    exc = LibException

    if rpc:
        exc = RpcException

    if error != <rpc_object_t>NULL:
        try:
            raise exc(rpc_error_get_code(error), rpc_error_get_message(error).decode('utf-8'))
        finally:
            free(<void *>error)

    raise exc(errno.EFAULT, "Unknown error")


def uint(value):
    return Object(value, force_type=ObjectType.UINT64)


def fd(value):
    return Object(value, force_type=ObjectType.FD)
