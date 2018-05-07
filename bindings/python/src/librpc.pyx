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
import functools
import traceback
import datetime
import uuid
from cpython.ref cimport Py_INCREF, Py_DECREF
from librpc cimport *
from libc.string cimport strdup
from libc.stdint cimport *
from libc.stdlib cimport malloc, free


cdef extern from "Python.h" nogil:
    void PyEval_InitThreads()


include "object.pxi"
include "connection.pxi"
include "service.pxi"
include "client.pxi"
include "server.pxi"
include "bus.pxi"
include "serializer.pxi"
include "typing.pxi"


cdef str_or_none(const char *val):
    if val == NULL:
        return None

    return val.decode('utf-8')


cdef const char *cstr_or_null(val):
    if not val:
        return NULL

    return val.encode('utf-8')


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


def uint(value):
    return Object(value, force_type=ObjectType.UINT64)


def fd(value):
    return Object(value, force_type=ObjectType.FD)


def method(arg):
    def wrapped(fn):
        fn.__librpc_name__ = arg
        fn.__librpc_method__ = True
        return fn

    if callable(arg):
        arg.__librpc_name__ = arg.__name__
        arg.__librpc_method__ = True
        return arg

    return wrapped


def prop(arg):
    def wrapped(fn):
        def setter(sfn):
            fn.__librpc_setter__ = sfn

        fn.setter = setter
        fn.__libpc_name__ = arg
        fn.__librpc_getter__ = True
        return fn

    if callable(arg):
        def setter(fn):
            arg.__librpc_setter__ = fn

        arg.setter = setter
        arg.__librpc_name__ = arg.__name__
        arg.__librpc_getter__ = True
        return arg

    return wrapped


def interface(name):
    def wrapped(fn):
        fn.__librpc_interface__ = name
        return fn

    return wrapped


def interface_base(name):
    def wrapped(fn):
        fn.__librpc_interface_base__ = name
        return fn

    return wrapped


def description(name):
    def wrapped(fn):
        fn.__librpc_description__ = name
        return fn

    return wrapped


def unpack(fn):
    fn.__librpc_unpack__ = True
    return fn


def new(decl, *args, **kwargs):
    return TypeInstance(decl).factory(*args, **kwargs)


type_hooks = {}
