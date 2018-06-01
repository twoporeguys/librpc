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

cdef class Context(object):
    def __init__(self):
        PyEval_InitThreads()
        self.methods = {}
        self.instances = {}
        self.context = rpc_context_create()

    @staticmethod
    cdef Context wrap(rpc_context_t ptr):
        cdef Context ret

        ret = Context.__new__(Context)
        ret.methods = {}
        ret.instances = {}
        ret.borrowed = True
        ret.context = ptr
        return ret

    cdef rpc_context_t unwrap(self) nogil:
        return self.context

    def register_instance(self, obj):
        cdef Instance instance

        if isinstance(obj, Service):
            instance = <Instance>obj.instance

        if isinstance(obj, Instance):
            instance = <Instance>obj

        self.instances[instance.path] = instance
        if rpc_context_register_instance(self.context, instance.instance) != 0:
            raise_internal_exc()

    def register_method(self, name, fn, interface=None):
        self.methods[name] = fn
        if rpc_context_register_func(
            self.context,
            cstr_or_null(interface),
            name.encode('utf-8'),
            <void *>fn,
            <rpc_function_f>c_cb_function
        ) != 0:
            raise_internal_exc()

    def unregister_member(self, name, interface=None):
        del self.methods[name]
        rpc_context_unregister_member(self.context, interface, name)

    def __dealloc__(self):
        if not self.borrowed:
            rpc_context_free(self.context)


cdef class Instance(object):
    @staticmethod
    cdef Instance wrap(rpc_instance_t ptr):
        cdef Instance result

        result = Instance.__new__(Instance)
        result.properties = []
        result.instance = ptr
        return result

    cdef rpc_instance_t unwrap(self) nogil:
        return self.instance

    @staticmethod
    cdef rpc_object_t c_property_getter(void *cookie) with gil:
        cdef object mux

        mux = <object>rpc_property_get_arg(cookie)
        getter, _ = mux

        try:
            result = getter()
            if not isinstance(result, Object):
                raise ValueError('returned value is not librpc.Object')

            return rpc_retain((<Object>result).obj)
        except RpcException as err:
            rpc_property_error(cookie, err.code, err.message.encode('utf-8'))
            return <rpc_object_t>NULL
        except Exception as err:
            rpc_property_error(cookie, errno.EINVAL, str(err).encode('utf-8'))
            return <rpc_object_t>NULL

    @staticmethod
    cdef void c_property_setter(void *cookie, rpc_object_t value) with gil:
        cdef object mux

        mux = <object>rpc_property_get_arg(cookie)
        _, setter = mux

        try:
            setter(Object.wrap(value))
        except RpcException as err:
            rpc_property_error(cookie, err.code, err.message.encode('utf-8'))
        except Exception as err:
            rpc_property_error(cookie, errno.EINVAL, str(err).encode('utf-8'))

    def __init__(self, path, description=None):
        b_path = path.encode('utf-8')
        self.instance = rpc_instance_new(NULL, b_path)
        self.properties = []

        if description:
            b_description = description.encode('utf-8')
            rpc_instance_set_description(self.instance, b_description)

    def register_interface(self, interface):
        b_interface = interface.encode('utf-8')
        if rpc_instance_register_interface(self.instance, b_interface, NULL, <void *>self) != 0:
            raise_internal_exc()

    def register_method(self, interface, name, fn):
        b_interface = interface.encode('utf-8')
        b_name = name.encode('utf-8')

        if rpc_instance_register_func(
            self.instance,
            b_interface,
            b_name,
            <void *>fn,
            <rpc_function_f>c_cb_function
        ) != 0:
            raise_internal_exc()

    def register_property(self, interface, name, getter=None, setter=None):
        b_interface = interface.encode('utf-8')
        b_name = name.encode('utf-8')
        mux = getter, setter

        self.properties.append(mux)

        if rpc_instance_register_property(
            self.instance,
            b_interface,
            b_name,
            <void *>mux,
            RPC_PROPERTY_GETTER(self.c_property_getter),
            RPC_PROPERTY_SETTER(self.c_property_setter)
        ) != 0:
            raise_internal_exc()

    def register_event(self, interface, name):
        pass

    def property_changed(self, interface, name):
        rpc_instance_property_changed(
            self.instance,
            interface.encode('utf-8'),
            name.encode('utf-8'),
            <rpc_object_t>NULL
        )

    property path:
        def __get__(self):
            return rpc_instance_get_path(self.instance).decode('utf-8')


cdef class Service(object):
    def __init__(self, path=None, description='Generic instance', instance=None):
        self.methods = {}
        self.properties = {}
        self.interfaces = {}
        self.instance = instance

        if not instance:
            self.instance = Instance(path, description)

        for name, i in inspect.getmembers(self, inspect.ismethod):
            if getattr(i, '__librpc_method__', None):
                name = getattr(i, '__librpc_name__', name)
                interface = getattr(i, '__librpc_interface__', getattr(self, '__librpc_interface__', None))
                description = getattr(self, '__librpc_description__', '')
                fn = i

                if not interface:
                    continue

                if interface not in self.interfaces:
                    self.instance.register_interface(interface)
                    self.interfaces[interface] = True

                self.instance.register_method(interface, name, fn)
                self.methods[name] = fn
                continue

            if getattr(i, '__librpc_getter__', None):
                name = getattr(i, '__librpc_name__', name)
                setter = getattr(i, '__librpc_setter__', None)
                interface = getattr(i, '__librpc_interface__', getattr(self, '__librpc_interface__', None))
                description = getattr(self, '__librpc_description__', '')
                getter = i

                if not interface:
                    continue

                # Setter is an unbound method reference, so we need to bind it to "self" first
                if setter:
                    setter = setter.__get__(self, self.__class__)

                if interface not in self.interfaces:
                    self.instance.register_interface(interface)
                    self.interfaces[interface] = True

                self.instance.register_property(interface, name, getter, setter)
                self.properties[name] = getter, setter
                continue

    property path:
        def __get__(self):
            return self.instance.path


def method(arg):
    def wrapped(fn):
        fn.__librpc_name__ = arg
        fn.__librpc_method__ = True
        return fn

    if callable(arg):
        arg.__librpc_name__ = arg.__name__
        arg.__librpc_method__ = True
        return arg

    return wrapped


def prop(arg):
    def wrapped(fn):
        def setter(sfn):
            fn.__librpc_setter__ = sfn
            return fn

        fn.setter = setter
        fn.__libpc_name__ = arg
        fn.__librpc_getter__ = True
        return fn

    if callable(arg):
        def setter(fn):
            arg.__librpc_setter__ = fn

        arg.setter = setter
        arg.__librpc_name__ = arg.__name__
        arg.__librpc_getter__ = True
        return arg

    return wrapped


def interface(name):
    def wrapped(fn):
        fn.__librpc_interface__ = name
        return fn

    return wrapped


def interface_base(name):
    def wrapped(fn):
        fn.__librpc_interface_base__ = name
        return fn

    return wrapped


def description(name):
    def wrapped(fn):
        fn.__librpc_description__ = name
        return fn

    return wrapped

