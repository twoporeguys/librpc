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

cdef class Client(Connection):


    def __init__(self):
        super(Client, self).__init__()
        self.uri = None
        self.client = <rpc_client_t>NULL
        self.connection = <rpc_connection_t>NULL

    @staticmethod
    cdef Client wrap(rpc_client_t ptr):
        cdef Client ret

        ret = Client.__new__(Client)
        ret.borrowed = True
        ret.connection = rpc_client_get_connection(ptr)
        return ret

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
