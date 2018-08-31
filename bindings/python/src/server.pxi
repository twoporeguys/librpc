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

cdef class Server(object):
    cdef rpc_server_t server
    cdef Context context
    cdef object uri

    def __init__(self, uri, context, params=None):
        if not uri:
            raise RuntimeError('URI cannot be empty')

        self.uri = uri.encode('utf-8')
        self.context = context
        self.server = rpc_server_create_ex(
            self.uri,
            self.context.unwrap(),
            Object(params).unwrap()
        )

        if self.server == <rpc_server_t>NULL:
            raise_internal_exc()

    @staticmethod
    cdef wrap(rpc_server_t c_server):
        cdef Server result

        result = Server.__new__(Server)
        result.server = c_server
        return result

    def broadcast_event(self, name, args, path='/', interface=None):
        cdef Object rpc_args
        cdef const char *c_name
        cdef const char *c_path
        cdef const char *c_interface = NULL

        rpc_args = Object(args)
        b_name = name.encode('utf-8')
        c_name = b_name
        b_path = path.encode('utf-8')
        c_path = b_path

        if interface:
            b_interface = interface.encode('utf-8')
            c_interface = b_interface

        with nogil:
            rpc_server_broadcast_event(
                self.server,
                c_path,
                c_interface,
                c_name,
                rpc_args.unwrap()
            )

    def resume(self):
        rpc_server_resume(self.server)

    def close(self):
        rpc_server_close(self.server)

    def __str__(self):
        return repr(self)

    def __repr__(self):
        return "<librpc.Server at '{0}'>".format(self.uri)

    IF SYSTEMD_SUPPORT:
        @staticmethod
        def systemd_listen(Context context):
            cdef Server server
            cdef int nservers
            cdef rpc_server_t *servers
            cdef rpc_object_t rest = <rpc_object_t>NULL
            cdef rpc_context_t c_context = context.unwrap()

            with nogil:
                nservers = rpc_server_sd_listen(c_context, &servers, &rest)

            if nservers < 0:
                raise_internal_exc()

            result = []
            for i in range(nservers):
                server = Server.wrap(servers[i])
                server.uri = None
                server.context = context
                result.append(server)

            return result, Object.wrap(rest).unpack()
