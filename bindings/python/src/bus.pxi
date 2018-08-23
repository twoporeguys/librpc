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

class BusEvent(enum.IntEnum):
    ATTACHED = RPC_BUS_ATTACHED
    DETACHED = RPC_BUS_DETACHED


@cython.internal
cdef class BusNode(object):
    cdef readonly object name
    cdef readonly object description
    cdef readonly object serial
    cdef readonly uint32_t address


cdef class Bus(object):
    def __init__(self):
        cdef int ret

        PyEval_InitThreads()
        with nogil:
            ret = rpc_bus_open()

        if ret != 0:
            raise_internal_exc()

    def __dealloc__(self):
        with nogil:
            rpc_bus_close()

    def ping(self, name):
        cdef const char *c_name
        cdef int ret

        b_name = name.encode('utf-8')
        c_name = b_name

        with nogil:
            ret = rpc_bus_ping(c_name)

        if ret < 0:
            raise_internal_exc()

    def enumerate(self):
        cdef BusNode node
        cdef rpc_bus_node *result
        cdef int ret

        with nogil:
            ret = rpc_bus_enumerate(&result)

        if ret < 0:
            raise_internal_exc()

        try:
            for i in range(0, ret):
                node = BusNode.__new__(BusNode)
                node.name = result[i].rbn_name.decode('utf-8')
                node.description = result[i].rbn_description.decode('utf-8')
                node.serial = result[i].rbn_serial.decode('utf-8')
                node.address = result[i].rbn_address
                yield node
        finally:
            rpc_bus_free_result(result)

    @staticmethod
    cdef void c_ev_handler(void *arg, rpc_bus_event_t ev, rpc_bus_node *bn) with gil:
        cdef object fn = <object>arg
        cdef BusNode node

        node = BusNode.__new__(BusNode)
        node.address = bn.rbn_address

        if bn.rbn_name:
            node.name = bn.rbn_name.decode('utf-8')

        if bn.rbn_description:
            node.description = bn.rbn_description.decode('utf-8')

        if bn.rbn_serial:
            node.serial = bn.rbn_serial.decode('utf-8')

        fn(BusEvent(ev), node)

    property event_handler:
        def __set__(self, value):
            if not value:
                rpc_bus_unregister_event_handler()
                self.event_fn = None
                return

            self.event_fn = value
            rpc_bus_register_event_handler(RPC_BUS_EVENT_HANDLER(
                self.c_ev_handler,
                <void *>self.event_fn
            ))

