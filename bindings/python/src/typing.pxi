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


DEFAULT_VALUES = {
    'nulltype': None,
    'bool': False,
    'uint64': uint(0),
    'int64': 0,
    'double': 0.0,
    'date': datetime.datetime.now(),
    'string': '',
    'binary': b'',
    'fd': fd(0),
    'error': None,
    'dictionary': Dictionary(),
    'array': Array()
}


class TypeClass(enum.IntEnum):
    STRUCT = RPC_TYPING_STRUCT
    UNION = RPC_TYPING_UNION
    ENUM = RPC_TYPING_ENUM
    TYPEDEF = RPC_TYPING_TYPEDEF
    CONTAINER = RPC_TYPING_CONTAINER
    BUILTIN = RPC_TYPING_BUILTIN


cdef class Typing(object):
    def __init__(self, load_system_types=True):
        cdef bint load = load_system_types

        with nogil:
            rpct_init(load)

    @staticmethod
    cdef bint c_type_iter(void *arg, rpct_type_t val):
        cdef Type rpctype
        cdef object container

        container = <object>arg
        rpctype = Type.__new__(Type)
        rpctype.rpctype = val
        container.append(rpctype)
        return True

    @staticmethod
    cdef bint c_iface_iter(void *arg, rpct_interface_t val):
        cdef Interface iface
        cdef object container

        container = <object>arg
        container.append(Interface.wrap(val))
        return True

    def load_type(self, decl, type):
        pass

    def read_file(self, path):
        if rpct_read_file(path.encode('utf-8')) != 0:
            raise_internal_exc()

    def load_types(self, path):
        if rpct_load_types(path.encode('utf-8')) != 0:
            raise_internal_exc()

    def load_types_dir(self, path):
        if rpct_load_types_dir(path.encode('utf-8')) != 0:
            raise_internal_exc()

    def download_types(self, Connection conn):
        if rpct_download_idl(conn.connection) != 0:
            raise_internal_exc()

    def serialize(self, Object obj):
        cdef rpc_object_t result

        result = rpct_serialize(obj.obj)
        return Object.wrap(result)

    def deserialize(self, Object obj):
        cdef rpc_object_t result

        result = rpct_deserialize(obj.obj)
        return Object.wrap(result)

    def find_interface(self, name):
        cdef rpct_interface_t c_iface

        c_iface = rpct_find_interface(name.encode('utf-8'))
        if c_iface == <rpct_interface_t>NULL:
            return None

        return Interface.wrap(c_iface)

    property types:
        def __get__(self):
            ret = []
            rpct_types_apply(RPCT_TYPE_APPLIER(self.c_type_iter, <void *>ret))
            return ret

    property interfaces:
        def __get__(self):
            ret = []
            rpct_interface_apply(RPCT_INTERFACE_APPLIER(self.c_iface_iter, <void *>ret))
            return ret

    property files:
        def __get__(self):
            pass


cdef class TypeInstance(object):
    def __init__(self, decl):
        self.rpctypei = rpct_new_typei(decl.encode('utf-8'))
        if self.rpctypei == <rpct_typei_t>NULL:
            raise_internal_exc()

    def __str__(self):
        return '<librpc.TypeInstance "{0}">'.format(self.canonical)

    def __repr__(self):
        return str(self)

    @staticmethod
    cdef TypeInstance wrap(rpct_typei_t typei):
        cdef TypeInstance ret

        if typei == <rpct_typei_t>NULL:
            return None

        ret = TypeInstance.__new__(TypeInstance)
        ret.rpctypei = typei
        return ret

    cdef rpct_typei_t unwrap(self) nogil:
        return self.rpctypei

    property factory:
        def __get__(self):
            if self.type.clazz == TypeClass.BUILTIN:
                def fn(x=None):
                    if not x:
                        x = DEFAULT_VALUES[self.canonical]

                    return Object(x).unpack()

                return fn

            if self.type.clazz == TypeClass.CONTAINER:
                if self.type.definition.canonical == 'array':
                    return Array

                if self.type.definition.canonical == 'dict':
                    return Dictionary

                raise AssertionError('Unknown container type')

            if self.type.clazz == TypeClass.STRUCT:
                return BaseTypingObject.construct_struct(self)

            if self.type.clazz == TypeClass.UNION:
                return BaseTypingObject.construct_union(self)

            if self.type.clazz == TypeClass.ENUM:
                return BaseTypingObject.construct_enum(self)

            if self.type.clazz == TypeClass.TYPEDEF:
                return self.type.definition.factory

    property proxy:
        def __get__(self):
            return rpct_typei_get_proxy(self.rpctypei)

    property proxy_variable:
        def __get__(self):
            return str_or_none(rpct_typei_get_proxy_variable(self.rpctypei))

    property type:
        def __get__(self):
            cdef Type typ

            typ = Type.__new__(Type)
            typ.rpctype = rpct_typei_get_type(self.rpctypei)
            return typ

    property canonical:
        def __get__(self):
            return rpct_typei_get_canonical_form(self.rpctypei).decode('utf-8')

    property generic_variables:
        def __get__(self):
            cdef TypeInstance typei
            cdef rpct_type_t base_type

            result = []
            base_type = rpct_typei_get_type(self.rpctypei)
            count = rpct_type_get_generic_vars_count(base_type)
            for i in range(count):
                typei = TypeInstance.__new__(TypeInstance)
                typei.rpctypei = rpct_typei_get_generic_var(self.rpctypei, rpct_type_get_generic_var(base_type, i))
                result.append(typei)

            return result

    def validate(self, Object obj):
        cdef rpc_object_t errors = <rpc_object_t>NULL
        cdef bint valid

        valid = rpct_validate(self.rpctypei, obj.obj, &errors)
        return valid, Object.wrap(errors)


cdef class Type(object):
    cdef rpct_type_t rpctype

    @staticmethod
    cdef bint c_iter(void *arg, rpct_member_t val):
        cdef Member member
        cdef object container

        container = <object>arg
        member = Member.__new__(Member)
        member.rpcmem = val
        container.append(member)
        return True

    def __str__(self):
        return "<librpc.Type '{0}'>".format(self.name)

    def __repr__(self):
        return str(self)

    property name:
        def __get__(self):
            return rpct_type_get_name(self.rpctype).decode('utf-8')

    property module:
        def __get__(self):
            return rpct_type_get_module(self.rpctype).decode('utf-8')

    property description:
        def __get__(self):
            return str_or_none(rpct_type_get_description(self.rpctype))

    property generic:
        def __get__(self):
            return rpct_type_get_generic_vars_count(self.rpctype) > 0

    property generic_variables:
        def __get__(self):
            ret = []
            count = rpct_type_get_generic_vars_count(self.rpctype)

            for i in range(count):
                ret.append(rpct_type_get_generic_var(self.rpctype, i).decode('utf-8'))

            return ret

    property clazz:
        def __get__(self):
            return TypeClass(rpct_type_get_class(self.rpctype))

    property members:
        def __get__(self):
            ret = []
            rpct_members_apply(self.rpctype, RPCT_MEMBER_APPLIER(self.c_iter, <void *>ret))
            return ret

    property definition:
        def __get__(self):
            return TypeInstance.wrap(rpct_type_get_definition(self.rpctype))

    property is_struct:
        def __get__(self):
            return self.clazz == TypeClass.STRUCT

    property is_union:
        def __get__(self):
            return self.clazz == TypeClass.UNION

    property is_enum:
        def __get__(self):
            return self.clazz == TypeClass.ENUM

    property is_typedef:
        def __get__(self):
            return self.clazz == TypeClass.TYPEDEF

    property is_builtin:
        def __get__(self):
            return self.clazz == TypeClass.BUILTIN


cdef class Member(object):
    cdef rpct_member_t rpcmem

    def __repr__(self):
        return str(self)

    def __str__(self):
        return "<{0} '{1}'>".format(self.__class__.__name__, self.name)

    property type:
        def __get__(self):
            return TypeInstance.wrap(rpct_member_get_typei(self.rpcmem))

    property name:
        def __get__(self):
            return rpct_member_get_name(self.rpcmem).decode('utf-8')

    property description:
        def __get__(self):
            return str_or_none(rpct_member_get_description(self.rpcmem))


cdef class Interface(object):
    property name:
        def __get__(self):
            return str_or_none(rpct_interface_get_name(self.c_iface))

    property description:
        def __get__(self):
            return str_or_none(rpct_interface_get_description(self.c_iface))

    property members:
        def __get__(self):
            ret = []
            rpct_if_member_apply(self.c_iface, RPCT_IF_MEMBER_APPLIER(self.c_iter, <void *>ret))
            return ret

    def __str__(self):
        return "<{0} '{1}'>".format(self.__class__.__name__, self.name)

    def __repr__(self):
        return str(self)

    def find_member(self, name):
        cdef rpct_if_member_t c_member

        c_member = rpct_find_if_member(self.name.encode('utf-8'), name.encode('utf-8'))
        if c_member == <rpct_if_member_t>NULL:
            return None

        return InterfaceMember.wrap(c_member)

    @staticmethod
    cdef Interface wrap(rpct_interface_t ptr):
        cdef Interface result

        result = Interface.__new__(Interface)
        result.c_iface = ptr
        return result

    cdef rpct_interface_t unwrap(self) nogil:
        return self.c_iface

    @staticmethod
    cdef bint c_iter(void *arg, rpct_if_member_t val):
        cdef object container = <object>arg

        container.append(InterfaceMember.wrap(val))
        return True


cdef class InterfaceMember(object):
    property name:
        def __get__(self):
            return str_or_none(rpct_if_member_get_name(self.c_member))

    property description:
        def __get__(self):
            return str_or_none(rpct_if_member_get_description(self.c_member))

    property access_flags:
        def __get__(self):
            pass

    @staticmethod
    cdef wrap(rpct_if_member_t ptr):
        cdef InterfaceMember result

        if rpct_if_member_get_type(ptr) == RPC_MEMBER_METHOD:
            result = Method.__new__(Method)
            result.c_member = ptr

        elif rpct_if_member_get_type(ptr) == RPC_MEMBER_PROPERTY:
            result = Property.__new__(Property)
            result.c_member = ptr

        elif rpct_if_member_get_type(ptr) == RPC_MEMBER_EVENT:
            result = Event.__new__(Event)
            result.c_member = ptr

        else:
            raise AssertionError('Impossible rpct_member_type')

        return result

    cdef rpct_if_member_t unwrap(self) nogil:
        return self.c_member


cdef class Method(InterfaceMember):
    property arguments:
        def __get__(self):
            cdef FunctionArgument arg

            result = []
            for i in range(0, rpct_method_get_arguments_count(self.c_member)):
                arg = FunctionArgument.__new__(FunctionArgument)
                arg.c_arg = rpct_method_get_argument(self.c_member, i)
                result.append(arg)

            return result

    property return_type:
        def __get__(self):
            return TypeInstance.wrap(rpct_method_get_return_type(self.c_member))

    def __str__(self):
        return "<librpc.Method name '{0}'>".format(self.name)

    def __repr__(self):
        return str(self)


cdef class Property(InterfaceMember):
    property type:
        def __get__(self):
            return TypeInstance.wrap(rpct_property_get_type(self.c_member))

    def __str__(self):
        return "<librpc.Property name '{0}'>".format(self.name)

    def __repr__(self):
        return str(self)


cdef class Event(InterfaceMember):
    property type:
        def __get__(self):
            return TypeInstance.wrap(rpct_property_get_type(self.c_member))

    def __str__(self):
        return "<librpc.Event name '{0}'>".format(self.name)

    def __repr__(self):
        return str(self)


cdef class FunctionArgument(object):
    cdef rpct_argument_t c_arg

    property description:
        def __get__(self):
            return str_or_none(rpct_argument_get_description(self.c_arg))

    property name:
        def __get__(self):
            return str_or_none(rpct_argument_get_name(self.c_arg))

    property type:
        def __get__(self):
            return TypeInstance.wrap(rpct_argument_get_typei(self.c_arg))


cdef class StructUnionMember(Member):
    def specialize(self, TypeInstance typei):
        return TypeInstance.wrap(rpct_typei_get_member_type(typei.rpctypei, self.rpcmem))


cdef class BaseTypingObject(object):
    __type_cache = {}

    def unpack(self):
        return self

    @staticmethod
    cdef construct_struct(TypeInstance typei):
        def getter(self, member):
            return self.object[member.name]

        def setter(self, member, value):
            value = Object(value, typei=member.type)
            result, errors = member.type.validate(value)
            if not result:
                raise LibException(errno.EINVAL, 'Validation failed', errors.unpack())

            self.object[member.name] = value

        def __init__(self, value=None, **kwargs):
            if not value:
                value = Dictionary(typei=self.typei)

            if isinstance(value, BaseStruct):
                value = value.object

            if value.typei.canonical != self.typei.canonical:
                raise TypeError('Incompatible value type')

            (<BaseTypingObject>self).object = value
            for m in self.members:
                if m.name not in self.object:
                    self.object[m.name] = m.type.factory(kwargs.get(m.name))

            result, errors = self.typei.validate(self.object)
            if not result:
                raise LibException(errno.EINVAL, 'Validation failed', errors.unpack())

        def __getitem__(self, item):
            return getattr(self, item)

        def __setitem__(self, key, value):
            setattr(self, key, value)

        def __str__(self):
            return "<struct {0}>".format(self.typei.type.name)

        def __repr__(self):
            return \
                '{0}('.format(self.typei.type.name) + \
                ', '.join('{0}={1!r}'.format(m.name, getattr(self, m.name)) for m in self.members) + \
                ')'

        cached = BaseTypingObject.__type_cache.get(typei.canonical)
        if cached:
            return cached

        members = {
            'typei': typei,
            'members': typei.type.members,
            '__init__': __init__,
            '__getitem__': __getitem__,
            '__setitem__': __setitem__,
            '__str__': __str__,
            '__repr__': __repr__
        }

        for m in typei.type.members:
            members[m.name] = property(
                lambda self, member=m: getter(self, member),
                lambda self, value, member=m: setter(self, member, value),
                None,
                m.description
            )

        ret = type(typei.type.name, (BaseStruct,), members)
        BaseTypingObject.__type_cache[typei.canonical] = ret
        return ret

    @staticmethod
    cdef construct_union(TypeInstance typei):
        def getter(self):
            return self.object

        def setter(self, value):
            value = Object(value, typei=self.typei)
            result, errors = self.typei.validate(value)
            if not result:
                raise LibException(errno.EINVAL, 'Validation failed', errors.unpack())

            (<BaseTypingObject>self).object = value

        def __init__(self, value):
            self.value = value

        def __str__(self):
            return "<union {0}>".format(self.typei.type.name)

        def __repr__(self):
            return '{0}({1})'.format(self.typei.type.name, self.value)

        cached = BaseTypingObject.__type_cache.get(typei.canonical)
        if cached:
            return cached

        members = {
            'typei': typei,
            'branches': {m.name: m for m in typei.type.members},
            'value': property(getter, setter),
            '__init__': __init__,
            '__str__': __str__,
            '__repr__': __repr__
        }

        ret = type(typei.type.name, (BaseUnion,), members)
        BaseTypingObject.__type_cache[typei.canonical] = ret
        return ret

    @staticmethod
    cdef construct_enum(TypeInstance typei):
        def getter(self):
            return self.object.unpack()

        def setter(self, value):
            value = Object(value, typei=self.typei)
            result, errors = self.typei.validate(value)
            if not result:
                raise LibException(errno.EINVAL, 'Validation failed', errors.unpack())

            (<BaseTypingObject>self).object = value

        def __str__(self):
            return "<enum {0}>".format(self.typei.type.name)

        def __repr__(self):
            return '{0}({1})'.format(self.typei.type.name, self.value)

        def __init__(self, value):
            self.value = value

        cached = BaseTypingObject.__type_cache.get(typei.canonical)
        if cached:
            return cached

        members = {
            'typei': typei,
            'values': [m.name for m in typei.type.members],
            'value': property(getter, setter),
            '__init__': __init__,
            '__str__': __str__,
            '__repr__': __repr__
        }

        ret = type(typei.type.name, (BaseEnum,), members)
        BaseTypingObject.__type_cache[typei.canonical] = ret
        return ret


cdef class BaseStruct(BaseTypingObject):
    pass


cdef class BaseUnion(BaseTypingObject):
    pass


cdef class BaseEnum(BaseTypingObject):
    pass


cdef class BaseType(BaseTypingObject):
    pass


def build(decl):
    return TypeInstance(decl).factory


def new(decl, *args, **kwargs):
    return TypeInstance(decl).factory(*args, **kwargs)
