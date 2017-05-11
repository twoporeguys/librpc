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
cimport defs


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

    def __init__(self, value):
        if value is None:
            self.obj = defs.rpc_null_create()
            return

        if isinstance(value, bool):
            self.obj = defs.rpc_bool_create(value)
            return

        if isinstance(value, int):
            self.obj = defs.rpc_int64_create(value)
            return

        if isinstance(value, str):
            self.obj = defs.rpc_string_create(value)
            return

        if isinstance(value, float):
            self.obj = defs.rpc_double_create(value)
            return

    def __dealloc__(self):
        defs.rpc_release(self.obj)

    property value:
        def __get__(self):
            pass

    property type:
        def __get__(self):
            return ObjectType(defs.rpc_get_type(self.obj))


cdef class Array(Object):
    def __getitem__(self, item):
        pass

    def __setitem__(self, key, value):
        pass

    def append(self, value):
        pass

    def extend(self, array):
        pass


cdef class Dictionary(Object):
    def __getitem__(self, item):
        pass

    def __setitem__(self, key, value):
        pass


cdef class Context(object):
    cdef defs.rpc_context_t context

    def register_method(self, name, description, fn):
        pass

    def unregister_method(self, name):
        pass


cdef class Connection(object):
    def call_sync(self, method, *args):
        pass

    def call_async(self, method, callback, *args):
        pass

    def emit_event(self, name, data):
        pass


cdef class Client(object):
    def connect(self, uri):
        pass


cdef class Server(object):
    pass
