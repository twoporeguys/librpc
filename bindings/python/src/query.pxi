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

def get(object, path, default=None):
    cdef rpc_object_t result

    rpc_obj = Object(object)
    rpc_default = Object(default)

    result = rpc_query_get(
        rpc_obj.unwrap(),
        cstr_or_null(path),
        rpc_default.unwrap()
    )

    return Object.wrap(result)


def set(object, path, value):
    rpc_obj = Object(object)
    rpc_val = Object(value)

    rpc_query_set(
        rpc_obj.unwrap(),
        cstr_or_null(path),
        rpc_val.unwrap(),
        False
    )

    return rpc_obj


def delete(object, path):
    rpc_obj = Object(object)

    rpc_query_delete(
        rpc_obj.unwrap(),
        cstr_or_null(path),
    )

    return rpc_obj


def contains(object, path):
    rpc_obj = Object(object)

    return rpc_query_contains(
        rpc_obj.unwrap(),
        cstr_or_null(path),
    )
