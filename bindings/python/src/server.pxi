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

    def resume(self):
        rpc_server_resume(self.server)

    def close(self):
        rpc_server_close(self.server)
