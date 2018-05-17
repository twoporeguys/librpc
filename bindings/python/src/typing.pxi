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

class TypeClass(enum.IntEnum):
    STRUCT = RPC_TYPING_STRUCT
    UNION = RPC_TYPING_UNION
    ENUM = RPC_TYPING_ENUM
    TYPEDEF = RPC_TYPING_TYPEDEF
    BUILTIN = RPC_TYPING_BUILTIN


cdef class Typing(object):
    def __init__(self):
        with nogil:
            rpct_init()

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
        iface = Interface.__new__(Interface)
        iface.c_iface = val
        container.append(iface)
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

    def serialize(self, Object obj):
        cdef rpc_object_t result

        result = rpct_serialize(obj.obj)
        return Object.init_from_ptr(result)

    def deserialize(self, Object obj):
        cdef rpc_object_t result

        result = rpct_deserialize(obj.obj)
        return Object.init_from_ptr(result)

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
            raise ValueError('Invalid type specifier')

    def __str__(self):
        return '<librpc.TypeInstance "{0}">'.format(self.canonical)

    def __repr__(self):
        return str(self)

    @staticmethod
    cdef TypeInstance init_from_ptr(rpct_typei_t typei):
        cdef TypeInstance ret

        if typei == <rpct_typei_t>NULL:
            return None

        ret = TypeInstance.__new__(TypeInstance)
        ret.rpctypei = typei
        return ret

    property factory:
        def __get__(self):
            if self.type.clazz == TypeClass.BUILTIN:
                return lambda x: Object(x).unpack()

            if self.type.clazz == TypeClass.STRUCT:
                return type(self.canonical, (BaseStruct,), {'typei': self})

            if self.type.clazz == TypeClass.UNION:
                return type(self.canonical, (BaseUnion,), {'typei': self})

            if self.type.clazz == TypeClass.ENUM:
                return type(self.canonical, (BaseEnum,), {'typei': self})

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

    def construct(self, Object obj):
        obj.obj = rpct_newi(self.rpctypei, obj.obj)
        return obj

    def validate(self, Object obj):
        cdef rpc_object_t errors = <rpc_object_t>NULL
        cdef bint valid

        valid = rpct_validate(self.rpctypei, obj.obj, &errors)
        return valid, Object.init_from_ptr(errors)


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
            return TypeInstance.init_from_ptr(rpct_type_get_definition(self.rpctype))

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
            return TypeInstance.init_from_ptr(rpct_member_get_typei(self.rpcmem))

    property name:
        def __get__(self):
            return rpct_member_get_name(self.rpcmem).decode('utf-8')

    property description:
        def __get__(self):
            return str_or_none(rpct_member_get_description(self.rpcmem))


cdef class Interface(object):
    cdef rpct_interface_t c_iface

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
        return "<{0} '{0}'>".format(self.__class__.__name__, self.name)

    def __repr__(self):
        return str(self)

    @staticmethod
    cdef bint c_iter(void *arg, rpct_if_member_t val):
        cdef rpc_if_member_type c_type
        cdef InterfaceMember result
        cdef object container = <object>arg

        c_type = rpct_if_member_get_type(val)

        if c_type == RPC_MEMBER_METHOD:
            result = Method.__new__(Method)

        elif c_type == RPC_MEMBER_PROPERTY:
            result = Property.__new__(Property)

        elif c_type == RPC_MEMBER_EVENT:
            result = Event.__new__(Event)
            print(result)

        result.c_member = val
        container.append(result)
        return True


cdef class InterfaceMember(object):
    cdef rpct_if_member_t c_member

    property name:
        def __get__(self):
            return str_or_none(rpct_if_member_get_name(self.c_member))

    property description:
        def __get__(self):
            return str_or_none(rpct_if_member_get_description(self.c_member))

    property access_flags:
        def __get__(self):
            pass


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
            return TypeInstance.init_from_ptr(rpct_method_get_return_type(self.c_member))

    def __str__(self):
        return "<librpc.Method name '{0}'>".format(self.name)

    def __repr__(self):
        return str(self)


cdef class Property(InterfaceMember):
    property type:
        def __get__(self):
            return TypeInstance.init_from_ptr(rpct_property_get_type(self.c_member))

    def __str__(self):
        return "<librpc.Property name '{0}'>".format(self.name)

    def __repr__(self):
        return str(self)


cdef class Event(InterfaceMember):
    property type:
        def __get__(self):
            return TypeInstance.init_from_ptr(rpct_property_get_type(self.c_member))

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
            return TypeInstance.init_from_ptr(rpct_argument_get_typei(self.c_arg))


cdef class StructUnionMember(Member):
    def specialize(self, TypeInstance typei):
        return TypeInstance.init_from_ptr(rpct_typei_get_member_type(typei.rpctypei, self.rpcmem))


cdef class BaseTypingObject(Object):
    def __init__(self, value):
        super(BaseTypingObject, self).__init__(value, typei=self.__class__.typei)
        result, errors = self.typei.validate(self)
        if not result:
            raise LibException(errno.EINVAL, 'Validation failed', errors.unpack())

    property members:
        def __get__(self):
            return {m.name: m for m in self.typei.type.members}



cdef class BaseStruct(BaseTypingObject):
    def __init__(self, __value=None, **kwargs):
        if not __value:
            __value = kwargs

        super(BaseStruct, self).__init__(__value)

    def __getitem__(self, item):
        return self.__getattr__(item)

    def __setitem__(self, key, value):
        self.__setattr__(key, value)

    def __getattr__(self, item):
        return self.value.value[item].unpack()

    def __setattr__(self, key, value):
        if not isinstance(value, Object):
            value = Object(value)

        member = self.members.get(key)
        if not member:
            raise LibException('Member {0} not found'.format(key))

        result, errors = member.type.validate(value)
        if not result:
            raise LibException(errno.EINVAL, 'Validation failed', errors.unpack())

        self.value.value[key] = value

    def __str__(self):
        return "<struct {0}>".format(self.typei.type.name)



cdef class BaseUnion(BaseTypingObject):
    def __init__(self, value):
        pass

    def __str__(self):
        return "<union {0}>".format(self.typei.type.name)


cdef class BaseEnum(BaseTypingObject):
    def __init__(self, value):
        super(BaseEnum, self).__init__(value)

    def __str__(self):
        return "<enum {0}>".format(self.typei.type.name)

    property value:
        def __get__(self):
            pass

    property values:
        def __get__(self):
            return [m.name for m in self.typei.type.members]


def build(decl):
    return TypeInstance(decl).factory


def new(decl, *args, **kwargs):
    return TypeInstance(decl).factory(*args, **kwargs)
