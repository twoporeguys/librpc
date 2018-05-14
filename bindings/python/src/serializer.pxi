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

cdef class Serializer(object):
    cdef const char *type

    def __init__(self, type):
        if not rpc_serializer_exists(type.encode('ascii')):
            raise LibException(errno.ENOENT, 'Unknown serializer')

        self.type = strdup(type.encode('ascii'))

    def __dealloc__(self):
        free(<void *>self.type)

    def loads(self, bytes blob, unpack=False):
        cdef Object ret
        cdef char *buf = blob
        cdef int length = len(blob)

        ret = Object.__new__(Object)

        with nogil:
            ret.obj = rpc_serializer_load(self.type, buf, length)

        if ret.obj == <rpc_object_t>NULL:
            raise_internal_exc()

        if unpack:
            return ret.unpack()

        return ret

    def dumps(self, obj):
        cdef void *frame
        cdef size_t len
        cdef int ret
        cdef Object rpc_obj

        if isinstance(obj, Object):
            rpc_obj = <Object>obj
        else:
            rpc_obj = Object(obj)

        with nogil:
            ret = rpc_serializer_dump(self.type, rpc_obj.obj, &frame, &len)

        if ret != 0:
            raise_internal_exc()

        return <bytes>(<char *>frame)[:len]
