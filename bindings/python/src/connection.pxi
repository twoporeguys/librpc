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
    STREAM_START = RPC_CALL_STREAM_START,
    MORE_AVAILABLE = RPC_CALL_MORE_AVAILABLE,
    DONE = RPC_CALL_DONE,
    ERROR = RPC_CALL_ERROR
    ABORTED = RPC_CALL_ABORTED
    ENDED = RPC_CALL_ENDED


cdef class Call(object):
    @staticmethod
    cdef Call wrap(rpc_call_t ptr):
        cdef Call result

        result = Call.__new__(Call)
        result.call = ptr
        return result

    cdef rpc_call_t unwrap(self) nogil:
        return self.call

    property status:
        def __get__(self):
            return CallStatus(rpc_call_status(self.call))

    property result:
        def __get__(self):
            with nogil:
                rpc_call_wait(self.call)

            return Object.wrap(rpc_call_result(self.call))

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

    property context:
        def __get__(self):
            cdef rpc_context_t c_context

            c_context = rpc_connection_get_context(self.connection)
            if c_context == <rpc_context_t>NULL:
                return None

            return Context.wrap(c_context)

        def __set__(self, Context value):
            rpc_connection_set_context(self.connection, value.unwrap())

    property instances:
        def __get__(self):
            result = {}
            objects = self.call_sync(
                'get_instances',
                interface='com.twoporeguys.librpc.Discoverable',
                path='/'
            )

            for o in objects:
                try:
                    inst = RemoteObject.construct(self, o['path'])
                    result[o['path']] = inst
                except LibException:
                    pass

            return result

    @staticmethod
    cdef Connection wrap(rpc_connection_t ptr):
        cdef Connection ret

        ret = Connection.__new__(Connection)
        ret.borrowed = True
        ret.connection = ptr
        return ret

    cdef rpc_connection_t unwrap(self) nogil:
        return self.connection

    @staticmethod
    cdef void c_ev_handler(
        void *arg, const char *path, const char *interface,
        const char *name, rpc_object_t args
    ) with gil:
        cdef Object event_args
        cdef object handler = <object>arg

        event_args = Object.wrap(args)
        handler(event_args.unpack())

    @staticmethod
    cdef void c_prop_handler(void *arg, rpc_object_t value) with gil:
        cdef object handler = <object>arg

        handler(Object.wrap(value).unpack())

    @staticmethod
    cdef void c_error_handler(void *arg, rpc_error_code_t error, rpc_object_t args) with gil:
        cdef Object error_args = None
        cdef object handler = <object>arg

        if args != <rpc_object_t>NULL:
            error_args = Object.wrap(args)

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

        result = Call.wrap(call)
        result.connection = self
        return result

    def call_sync(self, method, *args, path='/', interface=None):
        cdef rpc_object_t rpc_result
        cdef Array rpc_args
        cdef Dictionary error
        cdef rpc_call_t call
        cdef rpc_call_status_t call_status
        cdef const char *c_path
        cdef const char *c_interface = NULL
        cdef const char *c_method
        cdef int ret

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
            call = rpc_connection_call(
                self.connection,
                c_path,
                c_interface,
                c_method,
                rpc_retain(rpc_args.obj),
                NULL
            )

        if call == <rpc_call_t>NULL:
            raise_internal_exc(rpc=True)

        with nogil:
            ret = rpc_call_wait(call)

        if ret < 0:
            rpc_call_free(call)
            raise_internal_exc()

        call_status = rpc_call_status(call)

        def get_chunk():
            nonlocal rpc_result

            rpc_result = rpc_retain(rpc_call_result(call))
            return Object.wrap(rpc_result).unpack()

        def iter_chunk():
            nonlocal call_status

            try:
                if call_status == CallStatus.STREAM_START:
                    with nogil:
                        ret = rpc_call_continue(call, True)

                    if ret < 0:
                        raise_internal_exc()

                    call_status = rpc_call_status(call)

                while call_status == CallStatus.MORE_AVAILABLE:
                    yield get_chunk()
                    with nogil:
                        ret = rpc_call_continue(call, True)

                    if ret < 0:
                        raise_internal_exc()

                    call_status = rpc_call_status(call)
            finally:
                with nogil:
                    rpc_call_abort(call)
                    rpc_call_free(call)

        if call_status == CallStatus.ERROR:
            result = get_chunk()
            rpc_call_free(call)
            raise result

        if call_status == CallStatus.DONE:
            result = get_chunk()
            rpc_call_free(call)
            return result

        if call_status in (CallStatus.STREAM_START, CallStatus.MORE_AVAILABLE, CallStatus.ENDED):
            return iter_chunk()

        raise AssertionError('Impossible call status {0}'.format(call_status))

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

    def register_event_handler(self, name, fn, path='/', interface='com.twoporeguys.librpc.Default'):
        cdef void *cookie

        if self.connection == <rpc_connection_t>NULL:
            raise RuntimeError("Not connected")

        b_path = path.encode('utf-8')
        b_interface = interface.encode('utf-8')
        b_name = name.encode('utf-8')

        self.ev_handlers.append(fn)
        cookie = rpc_connection_register_event_handler(
            self.connection,
            b_path,
            b_interface,
            b_name,
            RPC_HANDLER(
                <rpc_handler_f>Connection.c_ev_handler,
                <void *>fn
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

        self.ev_handlers.append(fn)
        cookie = rpc_connection_watch_property(
            self.connection,
            b_path,
            b_interface,
            b_property,
            RPC_PROPERTY_HANDLER(
                <rpc_property_handler_f>Connection.c_prop_handler,
                <void *>fn
            )
        )

        if cookie == NULL:
            raise_internal_exc()

        return ListenHandle.init(self, cookie)


cdef class RemoteObject(object):
    @staticmethod
    def construct(client, path):
        def get_interfaces(self):
            ifaces = self.client.call_sync(
                'get_interfaces',
                path=self.path,
                interface='com.twoporeguys.librpc.Introspectable'
            )

            return {
                 i: RemoteInterface.construct(self.client, self, i)
                 for i in ifaces
            }

        def get_children(self):
            children = self.client.call_sync(
                'get_instances',
                path=self.path,
                interface='com.twoporeguys.librpc.Discoverable'
            )

            return {
                i['path']: RemoteObject.construct(self.client, i['path'])
                for i in children
            }

        def __str__(self):
            return "<librpc.RemoteObject at '{0}'>".format(self.path)

        members = {
            'client': client,
            'path': path,
            'name': os.path.basename(path),
            'description': None,
            'interfaces': property(get_interfaces),
            'children': property(get_children),
            '__str__': __str__,
            '__repr__': __str__
        }

        result = type(path, (RemoteObject,), members)

        for iface in result().interfaces.values():
            for name, method in iface.methods.items():
                if name in members:
                    continue

                if hasattr(result, name):
                    delattr(result, name)
                    continue

                setattr(result, name, method)

            for prop in iface.properties:
                if prop.name in members:
                    continue

                if hasattr(result, prop.name):
                    delattr(result, prop.name)
                    continue

                setattr(result, prop.name, property(prop.getter, prop.setter))

        return result()


cdef class RemoteProperty(object):
    def getter(self, parent):
        return parent.client.call_sync(
            'get',
            self.interface,
            self.name,
            path=parent.path,
            interface='com.twoporeguys.librpc.Observable'
         )

    def setter(self, parent, value):
        return parent.client.call_sync(
            'set',
            self.interface,
            self.name,
            value,
            path=parent.path,
            interface='com.twoporeguys.librpc.Observable'
         )

    def __str__(self):
        return repr(self)

    def __repr__(self):
        return "<librpc.RemoteProperty '{0}'>".format(self.name)


cdef class RemoteEvent(object):
    def connect(self, handler):
        if not self.handlers:
            pass

        self.handlers.append(handler)

    def disconnect(self, handler):
        self.handlers.remove(handler)
        if not self.handlers:
            pass

    def __str__(self):
        return repr(self)

    def __repr__(self):
        return "<librpc.RemoteEvent '{0}'>".format(self.name)

    cdef emit(self, Object args):
        for h in self.handlers:
            h(args.unpack())


cdef class RemoteInterface(object):
    __type_cache = {}

    @staticmethod
    def construct(client, instance, interface=''):
        cdef RemoteInterface result

        path = instance.path
        t = RemoteInterface.__type_cache.get(interface)

        if not t:
            try:
                if not client.call_sync(
                    'interface_exists',
                    interface,
                    path=path,
                    interface='com.twoporeguys.librpc.Introspectable'
                ):
                    raise ValueError('Interface not found')
            except:
                raise

            def call_sync(self, name, *args):
                return self.client.call_sync(
                    name, *args, path=self.path,
                    interface=self.interface
                )

            def watch_property(self, name, fn):
                return self.client.watch_property(
                    self.path, self.interface,
                    name, fn
                 )

            def __str__(self):
                return "<librpc.RemoteInterface '{0}' at '{1}'>".format(
                    self.interface,
                    self.path
                 )

            typed = Typing().find_interface(interface)
            methods = dict(RemoteInterface.__collect_methods(client, path, interface, typed))
            properties = list(RemoteInterface.__collect_properties(client, path, interface, typed))
            events = list(RemoteInterface.__collect_events(client, path, interface, typed))
            members = {
                'interface': interface,
                'typed': typed,
                'methods': methods,
                'properties': properties,
                'events': events,
                'call_sync': call_sync,
                'watch_property': watch_property,
                '__str__': __str__,
                '__repr__': __str__
            }

            for name, fn in methods.items():
                members[name] = fn

            for prop in properties:
                members[prop.name] = property(
                    lambda self, prop=prop: prop.getter(self),
                    lambda self, value, prop=prop: prop.setter(self, value)
                )

            for event in events:
                members[event.name] = event

            t = type(interface, (RemoteInterface,), members)
            RemoteInterface.__type_cache[interface] = t

        result = t.__new__(t)
        result.client = client
        result.instance = instance
        result.path = path
        return result

    @staticmethod
    def __collect_methods(client, path, interface, typed):
        try:
            for method in client.call_sync(
                'get_methods',
                interface,
                path=path,
                interface='com.twoporeguys.librpc.Introspectable'
            ):
                def fn(self, *args, __interface=interface, __method=method):
                    return self.client.call_sync(
                        __method,
                        *args,
                        path=self.path,
                        interface=__interface
                    )

                partial = fn
                partial.name = method
                partial.typed = None

                if typed:
                    partial.typed = typed.find_member(method)

                if partial.typed:
                    partial.__doc__ = partial.typed.description

                yield method, partial
        except:
            raise RuntimeError('Cannot read methods of a remote object')

    @staticmethod
    def __collect_properties(client, path, interface, typed):
        cdef RemoteProperty rprop

        try:
            for prop in client.call_sync(
                'get_all',
                interface,
                path=path,
                interface='com.twoporeguys.librpc.Observable'
            ):
                rprop = RemoteProperty.__new__(RemoteProperty)
                rprop.name = prop.name
                rprop.interface = interface
                rprop.typed = None

                if typed:
                    rprop.typed = typed.find_member(prop.name)

                yield rprop
        except:
            raise RuntimeError('Cannot read properties of a remote object')

    @staticmethod
    def __collect_events(client, path, interface, typed):
        cdef RemoteEvent revent

        try:
            for event in client.call_sync(
                'get_events',
                interface,
                path=path,
                interface='com.twoporeguys.librpc.Introspectable'
            ):
                revent = RemoteEvent.__new__(RemoteEvent)
                revent.handlers = []
                revent.name = event
                revent.interface = None
                yield revent
        except:
            raise RuntimeError('Cannot read events of a remote object')


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
            with nogil:
                ret = rpc_function_start_stream(cookie)

            if ret:
                return <rpc_object_t>NULL

            for chunk in output:
                rpc_obj = Object(chunk)

                with nogil:
                    ret = rpc_function_yield(cookie, rpc_retain(rpc_obj.unwrap()))

                if ret:
                    break

            return <rpc_object_t>NULL
    except Exception as e:
        if not isinstance(e, RpcException):
            e = RpcException(errno.EFAULT, str(e))

        rpc_obj = Object(e)
        rpc_function_error_ex(cookie, rpc_retain(rpc_obj.unwrap()))
        return <rpc_object_t>NULL

    rpc_obj = Object(output)
    return rpc_retain(rpc_obj.unwrap())
