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

import collections
cimport cpython.object


class ObjectType(enum.IntEnum):
    """
    Enumeration of possible object types.
    """
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
    """
    A boxed librpc object.
    """
    def __init__(self, value, typei=None):
        """
        Create a new boxed object.

        :param value: Value to box
        :param typei: Type instance to annotate the object
        """

        if value is None:
            self.obj = rpc_null_create()

        elif isinstance(value, bool):
            self.obj = rpc_bool_create(value)

        elif isinstance(value, fd):
            self.obj = rpc_fd_create(value)

        elif isinstance(value, uint):
            self.obj = rpc_uint64_create(value)

        elif isinstance(value, int):
            self.obj = rpc_int64_create(value)

        elif isinstance(value, str):
            bstr = value.encode('utf-8')
            self.obj = rpc_string_create(bstr)

        elif isinstance(value, float):
            self.obj = rpc_double_create(value)

        elif isinstance(value, datetime.datetime):
            self.obj = rpc_date_create(int(value.timestamp()))

        elif isinstance(value, (bytearray, bytes)):
            Py_INCREF(value)
            self.obj = rpc_data_create(
                <char *>value,
                <size_t>len(value),
                RPC_BINARY_DESTRUCTOR_ARG(destruct_bytes, <void *>value)
            )

        elif isinstance(value, (RpcException, LibException)):
            extra = Object(value.extra)
            stack = Object(value.stacktrace)
            self.obj = rpc_error_create_with_stack(value.code, value.message.encode('utf-8'), extra.obj, stack.obj)

        elif isinstance(value, (list, tuple)):
            self.obj = rpc_array_create()

            for v in value:
                child = Object(v)
                rpc_array_append_value(self.obj, child.obj)

        elif isinstance(value, dict):
            self.obj = rpc_dictionary_create()

            for k, v in value.items():
                bkey = k.encode('utf-8')
                child = Object(v)
                rpc_dictionary_set_value(self.obj, bkey, child.obj)

        elif isinstance(value, uuid.UUID):
            bstr = str(value).encode('utf-8')
            self.obj = rpc_string_create(bstr)

        elif isinstance(value, Object):
            self.obj = (<Object>value).obj
            rpc_retain(self.obj)

        elif hasattr(value, '__getstate__'):
            try:
                child = Object(value.__getstate__())
                self.obj = rpc_retain(child.obj)
            except:
                raise LibException(errno.EFAULT, "__getstate__() raised an exception")

        else:
            for typ, hook in type_hooks.items():
                if isinstance(value, typ):
                    try:
                        conv = hook(value)
                        if type(conv) is Object:
                            self.obj = rpc_copy((<Object>conv).obj)
                            break
                    except:
                        pass
            else:
                raise LibException(errno.EINVAL, "Unknown value type: {0}".format(type(value)))

        if typei:
            if isinstance(value, Object):
                self.obj = rpc_copy(self.obj)

            self.obj = rpct_set_typei((<TypeInstance>typei).rpctypei, self.obj)

    def __repr__(self):
        bdescr = rpc_copy_description(self.obj)
        result = bdescr.decode('utf-8')
        free(bdescr)
        return result

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
            return float(self.value)

        raise TypeError('float() argument must be a number or bool')

    def __richcmp__(Object self, Object other, int op):
        if op == cpython.object.Py_EQ:
            return rpc_equal(self.unwrap(), other.unwrap())

        if op != cpython.object.Py_NE:
            return not rpc_equal(self.unwrap(), other.unwrap())

    def __hash__(self):
        return rpc_hash(self.unwrap())

    def __dealloc__(self):
        if self.obj != <rpc_object_t>NULL:
            rpc_release(self.obj)

    @staticmethod
    cdef Object wrap(rpc_object_t ptr):
        cdef Object ret
        cdef rpct_typei_t typei

        if ptr == <rpc_object_t>NULL:
            return None

        if rpc_get_type(ptr) == RPC_TYPE_DICTIONARY:
            ret = Dictionary.__new__(Dictionary)

        elif rpc_get_type(ptr) == RPC_TYPE_ARRAY:
            ret = Array.__new__(Array)

        else:
            ret = Object.__new__(Object)

        ret.obj = rpc_retain(ptr)
        typei = rpct_get_typei(ptr)
        if typei != <rpct_typei_t>NULL and rpct_type_get_class(rpct_typei_get_type(typei)) != RPC_TYPING_BUILTIN:
            return TypeInstance.wrap(typei).factory(ret)

        return ret

    cdef rpc_object_t unwrap(self):
        return self.obj

    def unpack(self):
        """
        Unpack boxed value into a native Python value recursively.

        :param self:
        :return:
        """
        if isinstance(self, (BaseStruct, BaseUnion, BaseEnum)):
            return self

        if self.type in (ObjectType.FD, ObjectType.UINT64):
            return self

        return self.value

    def copy(self):
        return Object.wrap(rpc_copy(self.unwrap()))

    def validate(self):
        if not self.typei:
            raise LibException(errno.ENOENT, 'No type information')

        return self.typei.validate(self)

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
                return rpc_bool_get_value(self.unwrap())

            if self.type == ObjectType.INT64:
                return rpc_int64_get_value(self.unwrap())

            if self.type == ObjectType.UINT64:
                return rpc_uint64_get_value(self.unwrap())

            if self.type == ObjectType.FD:
                return rpc_fd_get_value(self.unwrap())

            if self.type == ObjectType.STRING:
                c_string = rpc_string_get_string_ptr(self.unwrap())
                c_len = rpc_string_get_length(self.unwrap())
                return c_string[:c_len].decode('utf-8')

            if self.type == ObjectType.DOUBLE:
                return rpc_double_get_value(self.unwrap())

            if self.type == ObjectType.DATE:
                return datetime.datetime.utcfromtimestamp(rpc_date_get_value(self.unwrap()))

            if self.type == ObjectType.BINARY:
                c_bytes = <uint8_t *>rpc_data_get_bytes_ptr(self.unwrap())
                c_len = rpc_data_get_length(self.unwrap())
                return <bytes>c_bytes[:c_len]

            if self.type == ObjectType.ERROR:
                extra = Object.wrap(rpc_error_get_extra(self.unwrap()))
                stack = Object.wrap(rpc_error_get_stack(self.unwrap()))

                return RpcException(
                    rpc_error_get_code(self.unwrap()),
                    rpc_error_get_message(self.unwrap()).decode('utf-8'),
                    extra,
                    stack
                )

            if self.type == ObjectType.ARRAY:
                array = Array.__new__(Array)
                array.obj = rpc_retain(self.unwrap())
                return array

            if self.type == ObjectType.DICTIONARY:
                dictionary = Dictionary.__new__(Dictionary)
                dictionary.obj = rpc_retain(self.unwrap())
                return dictionary

    property type:
        def __get__(self):
            return ObjectType(rpc_get_type(self.unwrap()))

    property typei:
        def __get__(self):
            return TypeInstance.wrap(rpct_get_typei(self.unwrap()))


cdef class Array(Object):
    def __init__(self, iterable=None, typei=None):
        if iterable is None:
            iterable = []

        if not isinstance(iterable, collections.Iterable):
            raise TypeError("'{0}' object is not iterable".format(type(iterable)))

        super(Array, self).__init__(iterable, typei=typei)

    @staticmethod
    cdef bint c_applier(void *arg, size_t index, rpc_object_t value) with gil:
        cdef object cb = <object>arg

        return <bint>cb(index, Object.wrap(value))

    @staticmethod
    cdef Array wrap(rpc_object_t ptr):
        cdef Array ret

        if ptr == <rpc_object_t>NULL:
            return None

        if rpc_get_type(ptr) != RPC_TYPE_ARRAY:
            return None

        ret = Array.__new__(Array)
        ret.obj = rpc_retain(ptr)
        return ret

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

        rpc_array_append_value(self.obj, Object(value).unwrap())

    def extend(self, array):
        cdef Object rpc_value
        cdef Array rpc_array

        if isinstance(array, Array):
            rpc_array = array
        elif isinstance(array, list):
            rpc_array = Array(array)
        else:
            raise TypeError('Array can be extended with only with list or another Array')

        for value in rpc_array:
            rpc_value = value
            rpc_array_append_value(self.obj, rpc_value.unwrap())

    def clear(self):
        rpc_release(self.obj)
        self.obj = rpc_array_create()

    def count(self, value):
        cdef Object v1
        cdef Object v2
        count = 0

        v1 = Object(value)

        def count_items(idx, v):
            nonlocal v2
            nonlocal count
            v2 = v

            if v1 == v2:
                count += 1

            return True

        self.__applier(count_items)
        return count

    def index(self, value):
        cdef Object v1
        cdef Object v2
        index = None

        v1 =  Object(value)

        def find_index(idx, v):
            nonlocal v2
            nonlocal index
            v2 = v

            if v1 == v2:
                index = idx
                return False

            return True

        self.__applier(find_index)

        if index is None:
            raise ValueError('{} is not in list'.format(value))

        return index

    def insert(self, index, value):
        rpc_array_set_value(self.obj, index, Object(value).unwrap())

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
            yield v.unpack()

    def __len__(self):
        return rpc_array_get_count(self.obj)

    def __sizeof__(self):
        return rpc_array_get_count(self.obj)

    def __getitem__(self, index):
        cdef rpc_object_t c_value
        cdef rpc_type_t c_type

        c_value = rpc_array_get_value(self.obj, index)
        if c_value == <rpc_object_t>NULL:
            raise IndexError('list index out of range')

        return Object.wrap(c_value).unpack()

    def __setitem__(self, index, value):
        cdef Object rpc_value

        rpc_value = Object(value)
        rpc_array_set_value(self.obj, index, rpc_value.obj)

    def __bool__(self):
        return len(self) > 0


cdef class Dictionary(Object):
    def __init__(self, mapping=None, typei=None, **kwargs):
        if mapping is None:
            mapping = {}

        if not isinstance(mapping, collections.Iterable):
            raise TypeError("'{0}' object is not iterable".format(type(mapping)))

        mapping.update(kwargs)
        super(Dictionary, self).__init__(mapping, typei=typei)

    @staticmethod
    cdef bint c_applier(void *arg, char *key, rpc_object_t value) with gil:
        cdef object cb = <object>arg

        return <bint>cb(key.decode('utf-8'), Object.wrap(value))

    @staticmethod
    cdef Dictionary wrap(rpc_object_t ptr):
        cdef Dictionary ret

        if ptr == <rpc_object_t>NULL:
            return None

        if rpc_get_type(ptr) != RPC_TYPE_DICTIONARY:
            return None

        ret = Dictionary.__new__(Dictionary)
        ret.obj = rpc_retain(ptr)
        return ret

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

    def get(self, k, d=None):
        try:
            return self.__getitem__(k)
        except KeyError:
            return d

    def items(self):
        result = []
        def collect(k, v):
            result.append((k, v.unpack()))
            return True

        self.__applier(collect)
        return result

    def keys(self):
        result = []
        def collect(k, _):
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

    def update(self, iterable=None, **kwargs):
        value = kwargs

        if iterable:
            if not isinstance(iterable, collections.Iterable):
                raise TypeError("'{0}' is not iterable".format(type(iterable)))

            kwargs.update(iterable)

        for k, v in value.items():
            rpc_dictionary_set_value(self.obj, k.encode('utf-8'), Object(v).unwrap())

    def values(self):
        result = []
        def collect(_, v):
            result.append(v.unpack())
            return True

        self.__applier(collect)
        return result

    def __str__(self):
        return repr(self)

    def __contains__(self, value):
        cdef Object v1
        cdef Object v2
        equal = False

        v1 = Object(value)

        def compare(k, v):
            nonlocal v2
            nonlocal equal
            v2 = v

            if v1 == v2:
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
        cdef rpc_object_t c_value
        byte_key = key.encode('utf-8')

        c_value = rpc_dictionary_get_value(self.obj, byte_key)
        if c_value == <rpc_object_t>NULL:
            raise KeyError(repr(key))

        return Object.wrap(c_value).unpack()

    def __setitem__(self, key, value):
        cdef Object rpc_value
        byte_key = key.encode('utf-8')

        rpc_value = Object(value)
        rpc_dictionary_set_value(self.obj, byte_key, rpc_value.obj)

    def __bool__(self):
        return len(self) > 0


cdef raise_internal_exc(rpc=False):
    cdef rpc_object_t error

    error = rpc_get_last_error()
    exc = LibException

    if rpc:
        exc = RpcException

    if error != <rpc_object_t>NULL:
        raise exc(rpc_error_get_code(error), rpc_error_get_message(error).decode('utf-8'))

    raise exc(errno.EFAULT, "Unknown error")


cdef void destruct_bytes(void *arg, void *buffer) with gil:
    cdef object value = <object>arg
    Py_DECREF(value)


class uint(int):
    def __init__(self, x=0, base=None):
        if base:
            v = int(x, base)
        else:
            v = int(x)

        if v < 0:
            raise ValueError('unsigned int value cannot be negative')

        super().__init__()

    def __iadd__(self, other):
        return uint(self + other)

    def __isub__(self, other):
        return uint(self - other)

    def __imul__(self, other):
        return uint(self * other)

    def __itruediv__(self, other):
        return uint(self // other)

    def __ifloordiv__(self, other):
        return uint(self // other)

    def __imod__(self, other):
        return uint(self % other)

    def __iand__(self, other):
        return uint(self & other)

    def __ior__(self, other):
        return uint(self | other)

    def __ixor__(self, other):
        return uint(self ^ other)

    def __ilshift__(self, other):
        return uint(self << other)

    def __irshift__(self, other):
        return uint(self >> other)

    def __ipow__(self, other):
        return uint(self ** other)

    def __repr__(self):
        return 'librpc.uint({0})'.format(self)


class fd(int):
    def __init__(self, x):
        if x < 0:
            raise ValueError('fd value cannot be negative')

        super().__init__()

    def __repr__(self):
        return 'librpc.fd({0})'.format(self)

