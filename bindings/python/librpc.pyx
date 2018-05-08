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


include "src/object.pxi"
include "src/connection.pxi"
include "src/service.pxi"
include "src/client.pxi"
include "src/server.pxi"
include "src/bus.pxi"
include "src/serializer.pxi"
include "src/typing.pxi"


cdef str_or_none(const char *val):
    if val == NULL:
        return None

    return val.decode('utf-8')


cdef const char *cstr_or_null(val):
    if not val:
        return NULL

    return val.encode('utf-8')


type_hooks = {}
