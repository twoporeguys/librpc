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

cdef class RPCD(object):
    @staticmethod
    def connect_to(service_name):
        cdef const char *c_service_name
        cdef rpc_client_t c_client

        b_service_name = service_name.encode('utf-8')
        c_service_name = b_service_name

        with nogil:
            c_client = rpcd_connect_to(c_service_name)

        if c_client == <rpc_client_t>NULL:
            raise_internal_exc()

        return Client.wrap(c_client)

    @staticmethod
    def register(uri, service_name, description):
        cdef const char *c_uri
        cdef const char *c_service_name
        cdef const char *c_description
        cdef int ret

        b_uri = uri.encode('utf-8')
        b_service_name = service_name.encode('utf-8')
        b_description = description.encode('utf-8')

        c_service_name = b_service_name
        c_description = b_description
        c_uri = b_uri

        with nogil:
            ret = rpcd_register(c_uri, c_service_name, c_description)

        if ret != 0:
            raise_internal_exc()
