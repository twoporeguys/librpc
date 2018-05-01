/*
 * Copyright 2015-2017 Two Pore Guys, Inc.
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "../include/librpc.hh"

using namespace librpc;

Object::Object()
{
	m_value = rpc_null_create();
}

Object::Object(const std::nullptr_t value)
{
	m_value = rpc_null_create();
}

Object::Object(const Object &other)
{
	m_value = rpc_retain(other.m_value);
}

Object::Object(Object &&other) noexcept
{
	m_value = other.m_value;
	other.m_value = nullptr;
}

Object::Object(bool value)
{
	m_value = rpc_bool_create(value);
}

Object::Object(uint64_t value)
{
	m_value = rpc_uint64_create(value);
}

Object::Object(int64_t value)
{
	m_value = rpc_int64_create(value);
}

Object::Object(double value)
{
	m_value = rpc_double_create(value);
}

Object::Object(const char *value)
{
	m_value = rpc_string_create(value);
}

Object::Object(const std::string &value)
{
	m_value = rpc_string_create(value.c_str());
}

Object::Object(void *data, size_t len, rpc_binary_destructor_t dtor)
{
	m_value = rpc_data_create(data, len, dtor);
}

Object::Object(const std::map<std::string, Object> &dict)
{
	m_value = rpc_dictionary_create();

	for (auto &kv : dict) {
		rpc_dictionary_set_value(m_value, kv.first.c_str(),
		    kv.second.m_value);
	}
}

Object::Object(const std::vector<Object> &array)
{
	m_value = rpc_array_create();

	for (auto &v : array) {
		rpc_array_append_value(m_value, v.m_value);
	}
}

Object::Object(std::initializer_list<Object> list)
{
	m_value = rpc_array_create();

	for (auto &v : list) {
		rpc_array_append_value(m_value, v.m_value);
	}
}

Object::Object(std::initializer_list<std::pair<std::string, Object>> list)
{
	m_value = rpc_dictionary_create();

	for (auto &kv : list) {
		rpc_dictionary_set_value(m_value, kv.first.c_str(),
		    kv.second.m_value);
	}
}

Object::~Object()
{
	rpc_release(m_value);
}

Object
Object::wrap(rpc_object_t other)
{
	Object result;

	result.m_value = rpc_retain(other);
	return (result);
}

rpc_type_t
Object::type()
{
	return (rpc_get_type(m_value));
}

Object
Object::copy()
{
	return (Object::wrap(rpc_copy(m_value)));
}

void
Object::retain()
{
	rpc_retain(m_value);
}

void
Object::release()
{
	rpc_release(m_value);
}

rpc_object_t
Object::unwrap() const
{
	return (m_value);
}

std::string
Object::describe()
{
	std::string result;
	char *descr;

	descr = rpc_copy_description(m_value);
	result = descr;
	free(descr);

	return (result);
}

Object
Object::get(const std::string &key, const librpc::Object &def)
{
	rpc_object_t value;

	value = rpc_dictionary_get_value(m_value, key.c_str());
	if (value == nullptr)
		return (def);

	return (Object::wrap(value));
}

Object
Object::get(size_t index, const librpc::Object &def)
{
	rpc_object_t value;

	value = rpc_array_get_value(m_value, index);
	if (value == nullptr)
		return (def);

	return (Object::wrap(value));
}

void
Object::push_back(const librpc::Object &value)
{
	rpc_array_append_value(m_value, value.unwrap());
}

void
Object::set(const std::string &key, const librpc::Object &value)
{
	rpc_dictionary_set_value(m_value, key.c_str(), value.unwrap());
}

int
Object::get_error_code()
{
	return (rpc_error_get_code(m_value));
}

std::string
Object::get_error_message()
{
	return (rpc_error_get_message(m_value));
}

Object::operator int() const
{
	return ((int)rpc_int64_get_value(m_value));
}

Object::operator int64_t() const
{
	return (rpc_int64_get_value(m_value));
}

Object::operator uint64_t() const
{
	return (rpc_uint64_get_value(m_value));
}

Object::operator const char *() const
{
	return (rpc_string_get_string_ptr(m_value));
}

Object::operator std::string() const
{
	return (std::string(rpc_string_get_string_ptr(m_value)));
}

Object
Object::operator[](const std::string &key)
{
	return (Object::wrap(rpc_dictionary_get_value(m_value, key.c_str())));
}

Object
Object::operator[](size_t index)
{
	return (Object::wrap(rpc_array_get_value(m_value, index)));
}
