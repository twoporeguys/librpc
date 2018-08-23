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

@cython.internal
cdef class QueryIterator(object):
    def __dealloc__(self):
        if self.iter != <rpc_query_iter_t>NULL:
            rpc_query_iter_free(self.iter)

    def __iter__(self):
        return self

    def __next__(self):
        cdef rpc_object_t result

        if not self.cnt:
            raise StopIteration

        self.cnt = rpc_query_next(self.iter, &result)

        py_result = Object.wrap(result)

        if self.unpack:
            return py_result.unpack()

        return py_result

    def next(self):
        return self.__next__()

    @staticmethod
    cdef rpc_object_t c_callback(void *arg, rpc_object_t object):
        cdef object cb = <object>arg
        cdef Object result

        rpc_retain(object)
        result = cb(Object.wrap(object))
        return result.unwrap()

    @staticmethod
    cdef int c_sort(void *arg, rpc_object_t o1, rpc_object_t o2):
        cdef object cb = <object>arg

        rpc_retain(o1)
        rpc_retain(o2)
        return int(cb(Object.wrap(o1), Object.wrap(o2)))

    @staticmethod
    cdef QueryIterator wrap(rpc_query_iter_t iter, object sort, object cb, object unpack):
        cdef QueryIterator ret

        ret = QueryIterator.__new__(QueryIterator)
        ret.iter = iter
        ret.sort_cb = sort
        ret.postprocess_cb = cb
        ret.unpack = unpack
        ret.cnt = True

        return ret


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


def query(object, rules, **params):
    cdef rpc_query_params rpc_params
    cdef rpc_query_iter_t query_iter

    rpc_params.single = bool(params.pop('single', False))
    rpc_params.count = bool(params.pop('count', None))
    rpc_params.offset = int(params.pop('offset', 0))
    rpc_params.limit = int(params.pop('limit', 0))
    rpc_params.reverse = bool(params.pop('reverse', False))
    sort = params.pop('sort', None)
    postprocess = params.pop('callback', None)
    unpack = params.pop('unpack', None)

    rpc_params.sort = NULL
    rpc_params.callback = NULL

    if sort:
        rpc_params.sort = RPC_ARRAY_CMP(
            <rpc_array_cmp_f>QueryIterator.c_sort,
            <void *>sort,
        )

    if postprocess:
        rpc_params.callback = RPC_QUERY_CB(
            <rpc_query_cb_f>QueryIterator.c_callback,
            <void *>postprocess,
        )

    rpc_rules = Object(rules)
    rpc_obj = Object(object)

    query_iter = rpc_query(rpc_obj.unwrap(), &rpc_params, rpc_rules.unwrap())

    if query_iter == <rpc_query_iter_t>NULL:
        return None

    return QueryIterator.wrap(
        query_iter,
        sort,
        postprocess,
        unpack
    )
