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

class ErrorCode(enum.IntEnum):
    """
    Enumeration of connection error codes known to the library.
    """
    INVALID_RESPONSE = RPC_INVALID_RESPONSE
    CONNECTION_TIMEOUT = RPC_CONNECTION_TIMEOUT
    CONNECTION_CLOSED = RPC_CONNECTION_CLOSED
    CALL_TIMEOUT = RPC_CALL_TIMEOUT
    SPURIOUS_RESPONSE = RPC_SPURIOUS_RESPONSE
    LOGOUT = RPC_LOGOUT
    OTHER = RPC_OTHER


class CallStatus(enum.IntEnum):
    """
    Enumeration of possible call object states.
    """
    IN_PROGRESS = RPC_CALL_IN_PROGRESS,
    MORE_AVAILABLE = RPC_CALL_MORE_AVAILABLE,
    DONE = RPC_CALL_DONE,
    ERROR = RPC_CALL_ERROR
    ABORTED = RPC_CALL_ABORTED
    ENDED = RPC_CALL_ENDED


cdef class Call(object):
    @staticmethod
    cdef Call init_from_ptr(rpc_call_t ptr):
        cdef Call result

        result = Call.__new__(Call)
        result.call = ptr
        return result

    property status:
        def __get__(self):
            return CallStatus(rpc_call_status(self.call))

    property result:
        def __get__(self):
            with nogil:
                rpc_call_wait(self.call)

            return Object.init_from_ptr(rpc_call_result(self.call))

    def __dealloc__(self):
        rpc_call_free(self.call)

    def __iter__(self):
        return self

    def __next__(self):
        result = self.result

        if result is None:
            raise StopIteration()

        if result.type == ObjectType.ERROR:
            raise result.value

        self.resume()
        return result

    def abort(self):
        with nogil:
            rpc_call_abort(self.call)

    def resume(self):
        with nogil:
            rpc_call_continue(self.call, True)


cdef class ListenHandle(object):
    cdef readonly Connection connection
    cdef void *c_cookie

    @staticmethod
    cdef init(Connection conn, void *c_cookie):
        cdef ListenHandle result

        result = ListenHandle.__new__(ListenHandle)
        result.connection = conn
        result.c_cookie = c_cookie
        return result

    def unregister(self):
        rpc_connection_unregister_event_handler(
            self.connection.connection,
            self.c_cookie
        )


cdef class Connection(object):
    def __init__(self):
        PyEval_InitThreads()
        self.error_handler = None
        self.event_handler = None
        self.unpack = False
        self.ev_handlers = []

    property error_handler:
        def __get__(self):
            return self.error_handler

        def __set__(self, fn):
            if not callable(fn):
                fn = None

            self.error_handler = fn
            rpc_connection_set_error_handler(
                self.connection,
                RPC_ERROR_HANDLER(
                    self.c_error_handler,
                    <void *>self.error_handler
                )
            )

    property event_handler:
        def __get__(self):
            return self.event_handler

        def __set__(self, fn):
            self.event_handler = fn

    property instances:
        def __get__(self):
            objects = self.call_sync(
                'get_instances',
                interface='com.twoporeguys.librpc.Discoverable',
                path='/',
                unpack=True
            )

            return {o['path']: RemoteObject(self, o['path']) for o in objects}

    @staticmethod
    cdef Connection init_from_ptr(rpc_connection_t ptr):
        cdef Connection ret

        ret = Connection.__new__(Connection)
        ret.borrowed = True
        ret.connection = ptr
        return ret

    @staticmethod
    cdef void c_ev_handler(
        void *arg, const char *path, const char *interface,
        const char *name, rpc_object_t args
    ) with gil:
        cdef Object event_args
        cdef object handler = <object>arg

        event_args = Object.init_from_ptr(args)
        handler(event_args)

    @staticmethod
    cdef void c_prop_handler(void *arg, rpc_object_t value) with gil:
        cdef object handler = <object>arg

        handler(Object.init_from_ptr(value))

    @staticmethod
    cdef void c_error_handler(void *arg, rpc_error_code_t error, rpc_object_t args) with gil:
        cdef Object error_args = None
        cdef object handler = <object>arg

        if args != <rpc_object_t>NULL:
            error_args = Object.init_from_ptr(args)

        handler(ErrorCode(error), error_args)

    def call(self, const char *path, const char *interface, const char *method, *args):
        cdef Array rpc_args
        cdef Call result

        if self.connection == <rpc_connection_t>NULL:
            raise RuntimeError("Not connected")

        rpc_args = Array(list(args))
        rpc_retain(rpc_args.obj)

        with nogil:
            call = rpc_connection_call(self.connection, path, interface, method, rpc_args.obj, NULL)

        if call == <rpc_call_t>NULL:
            raise_internal_exc(rpc=True)

        result = Call.init_from_ptr(call)
        result.connection = self
        return result

    def call_sync(self, method, *args, path='/', interface=None, unpack=None):
        cdef rpc_object_t rpc_result
        cdef Array rpc_args
        cdef Dictionary error
        cdef rpc_call_t call
        cdef rpc_call_status_t call_status
        cdef const char *c_path
        cdef const char *c_interface = NULL
        cdef const char *c_method

        if self.connection == <rpc_connection_t>NULL:
            raise RuntimeError("Not connected")

        rpc_args = Array(list(args))
        b_path = path.encode('utf-8')
        c_path = b_path
        b_method = method.encode('utf-8')
        c_method = b_method

        if interface:
            b_interface = interface.encode('utf-8')
            c_interface = b_interface

        with nogil:
            call = rpc_connection_call(self.connection, c_path, c_interface, c_method, rpc_args.obj, NULL)

        if call == <rpc_call_t>NULL:
            raise_internal_exc(rpc=True)

        with nogil:
            rpc_call_wait(call)

        call_status = rpc_call_status(call)

        def get_chunk():
            nonlocal rpc_result

            rpc_result = rpc_call_result(call)
            rpc_retain(rpc_result)

            return self.do_unpack(Object.init_from_ptr(rpc_result), unpack)

        def iter_chunk():
            nonlocal call_status

            try:
                while call_status == CallStatus.MORE_AVAILABLE:
                    yield get_chunk()
                    with nogil:
                        rpc_call_continue(call, True)

                    call_status = rpc_call_status(call)
            finally:
                with nogil:
                    rpc_call_abort(call)

        if call_status == CallStatus.ERROR:
            raise self.do_unpack(get_chunk(), True)

        if call_status == CallStatus.DONE:
            return get_chunk()

        if call_status in (CallStatus.MORE_AVAILABLE, CallStatus.ENDED):
            return iter_chunk()

        raise AssertionError('Impossible call status {0}'.format(call_status))

    def do_unpack(self, value, unpack=None):
        if not isinstance(value, Object):
            return value

        unpack = self.unpack if unpack is None else unpack
        return value.unpack() if unpack else value

    def call_async(self, method, callback, *args):
        pass

    def emit_event(self, name, Object data, path='/', interface=None):
        cdef const char *c_path
        cdef const char *c_name
        cdef const char *c_interface = NULL

        b_path = path.encode('utf-8')
        c_path = b_path
        b_name = name.encode('utf-8')
        c_name = b_name

        if interface:
            b_interface = interface.encode('utf-8')
            c_interface = b_interface

        with nogil:
            rpc_connection_send_event(self.connection, c_path, c_interface, c_name, data.obj)

    def register_event_handler(self, name, fn, path='/', interface=None):
        cdef void *cookie

        if self.connection == <rpc_connection_t>NULL:
            raise RuntimeError("Not connected")

        b_path = path.encode('utf-8')
        b_interface = path.encode('utf-8')
        b_name = name.encode('utf-8')
        fn_unpack = lambda val: fn(self.do_unpack(val))

        self.ev_handlers.append(fn)
        cookie = rpc_connection_register_event_handler(
            self.connection,
            b_path,
            b_interface,
            b_name,
            RPC_HANDLER(
                <rpc_handler_f>Connection.c_ev_handler,
                <void *>fn_unpack
            )
        )

        if cookie == NULL:
            raise_internal_exc()

        return ListenHandle.init(self, cookie)

    def watch_property(self, path, interface, property, fn):
        cdef void *cookie

        if self.connection == <rpc_connection_t>NULL:
            raise RuntimeError("Not connected")

        b_path = path.encode('utf-8')
        b_interface = interface.encode('utf-8')
        b_property = property.encode('utf-8')
        fn_unpack = lambda val: fn(self.do_unpack(val))

        self.ev_handlers.append(fn_unpack)
        cookie = rpc_connection_watch_property(
            self.connection,
            b_path,
            b_interface,
            b_property,
            RPC_PROPERTY_HANDLER(
                <rpc_property_handler_f>Connection.c_prop_handler,
                <void *>fn_unpack
            )
        )

        if cookie == NULL:
            raise_internal_exc()

        return ListenHandle.init(self, cookie)


cdef class RemoteObject(object):
    def __init__(self, client, path):
        self.client = client
        self.path = path

    property path:
        def __get__(self):
            return self.path

    property description:
        def __get__(self):
            pass

    property interfaces:
        def __get__(self):
            ifaces = self.client.call_sync(
                'get_interfaces',
                path=self.path,
                interface='com.twoporeguys.librpc.Introspectable',
                unpack=True
            )

            return {i: RemoteInterface(self.client, self.path, i) for i in ifaces}

    property children:
        def __get__(self):
            children = self.client.call_sync(
                'get_instances',
                path=self.path,
                interface='com.twoporeguys.librpc.Discoverable',
                unpack=True
            )

            return {i['path']: RemoteObject(self.client, i['path']) for i in children}

    def __str__(self):
        return "<librpc.RemoteObject at '{0}'>".format(self.path)

    def __repr__(self):
        return str(self)


cdef class RemoteInterface(object):
    def __init__(self, client, path, interface=''):
        self.client = client
        self.path = path
        self.name = os.path.basename(path)
        self.interface = interface
        self.methods = {}
        self.properties = {}
        self.events = {}
        self.typed = Typing().find_interface(interface)

        try:
            if not self.client.call_sync(
                'interface_exists',
                interface,
                path=path,
                interface='com.twoporeguys.librpc.Introspectable'
            ):
                raise ValueError('Interface not found')
        except:
            raise

        self.__collect_methods()
        self.__collect_properties()

    def call_sync(self, name, *args):
        return self.client.call_sync(name, *args, path=self.path, interface=self.interface)

    def watch_property(self, name, fn):
        return self.client.watch_property(self.path, self.interface, name, fn)

    def __collect_methods(self):
        try:
            for method in self.client.call_sync(
                'get_methods',
                self.interface,
                path=self.path,
                interface='com.twoporeguys.librpc.Introspectable',
                unpack=True
            ):
                def fn(method, *args):
                    return self.client.call_sync(
                        method,
                        *args,
                        path=self.path,
                        interface=self.interface
                    )

                partial = functools.partial(fn, method)
                self.methods[method] = partial
                setattr(self, method, partial)
        except:
            raise RuntimeError('Cannot read methods of a remote object')

    def __collect_properties(self):
        try:
            for prop in self.client.call_sync(
                'get_all',
                self.interface,
                path=self.path,
                interface='com.twoporeguys.librpc.Observable',
                unpack=True
            ):
                def getter(name=prop['name']):
                    return self.client.call_sync(
                        'get',
                        self.interface,
                        name,
                        path=self.path,
                        interface='com.twoporeguys.librpc.Observable'
                    )

                def setter(name=prop['name'], value=None):
                    return self.client.call_sync(
                        'set',
                        self.interface,
                        name,
                        value,
                        path=self.path,
                        interface='com.twoporeguys.librpc.Observable'
                    )

                self.properties[prop['name']] = (
                    functools.partial(getter, prop['name']),
                    functools.partial(setter, prop['name'])
                )
        except:
            raise RuntimeError('Cannot read properties of a remote object')

    def __collect_events(self):
            for method in self.client.call_sync(
                'get_events',
                self.interface,
                path=self.path,
                interface='com.twoporeguys.librpc.Introspectable',
                unpack=True
            ):
                self.client.register_event_handler()

    def __getattr__(self, item):
        getter, _ = self.properties[item]
        return getter()

    def __setattr__(self, item, value):
        if item in self.properties:
            _, setter = self.properties[item]
            setter(value)
            return

        self.__dict__[item] = value

    def __str__(self):
        return "<librpc.RemoteInterface '{0}' at '{1}'>".format(
            self.interface,
            self.path
        )

    def __repr__(self):
        return str(self)


cdef class RemoteEvent(object):
    def connect(self, handler):
        self.handlers.append(handler)

    def disconnect(self, handler):
        self.handlers.remove(handler)

    cdef emit(self, name, Object args):
        for h in self.handlers:
            h(args)






cdef rpc_object_t c_cb_function(void *cookie, rpc_object_t args) with gil:
    cdef Array args_array
    cdef Object rpc_obj
    cdef object cb = <object>rpc_function_get_arg(cookie)
    cdef int ret

    args_array = Array.__new__(Array)
    args_array.obj = args

    try:
        output = cb(*[a for a in args_array])
        if isinstance(output, types.GeneratorType):
            for chunk in output:
                rpc_obj = Object(chunk)
                rpc_retain(rpc_obj.obj)

                with nogil:
                    ret = rpc_function_yield(cookie, rpc_obj.obj)

                if ret:
                    break

            return <rpc_object_t>NULL
    except Exception as e:
        if not isinstance(e, RpcException):
            e = RpcException(errno.EFAULT, str(e))

        rpc_obj = Object(e)
        rpc_function_error_ex(cookie, rpc_obj.obj)
        return <rpc_object_t>NULL

    rpc_obj = Object(output)
    rpc_retain(rpc_obj.obj)
    return rpc_obj.obj

