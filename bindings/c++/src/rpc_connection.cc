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

#include <cerrno>
#include <functional>
#include <exception>
#include <rpc/connection.h>
#include <rpc/client.h>
#include "../include/librpc.hh"

using namespace librpc;

Exception::Exception(int code, const std::string &message):
    std::runtime_error(message)
{
	m_code = code;
	m_message = message;
}

Exception
Exception::last_error()
{
	Object error = Object::wrap(rpc_get_last_error());

	return (Exception(error.get_error_code(), error.get_error_message()));
}

int
Exception::code()
{
	return (m_code);
}

const std::string &
Exception::message()
{
	return (m_message);
}

Call::~Call()
{
	rpc_call_free(m_call);
}

Object
Call::result() const
{
	return (Object::wrap(rpc_call_result(m_call)));
}

enum rpc_call_status
Call::status() const
{
	return ((enum rpc_call_status)rpc_call_status(m_call));
}

void
Call::resume(bool sync)
{
	rpc_call_continue(m_call, sync);
}

void
Call::wait()
{
	rpc_call_wait(m_call);
}

void
Call::abort()
{
	rpc_call_abort(m_call);
}

Call
Call::wrap(rpc_call_t other)
{
	Call result;

	result.m_call = other;
	return (result);
}

Call
Connection::call(const std::string &name, const std::vector<Object> &args,
    const std::string &path, const std::string &interface)
{
	Object wrapped(args);

	rpc_call_t call = rpc_connection_call(m_connection, path.c_str(),
	    interface.c_str(), name.c_str(), wrapped.unwrap(), nullptr);

	if (call == nullptr)
		throw (Exception::last_error());

	return (Call::wrap(call));
}

Object
Connection::call_sync(const std::string &name, const std::vector<Object> &args,
    const std::string &path, const std::string &interface)
{
	Object wrapped(args);

	Call c = call(name, args, path, interface);
	c.wait();
	return (c.result());
}

void
Connection::call_async(const std::string &name, const std::vector<Object> &args,
    const std::string &path, const std::string &interface,
    std::function<bool (Call)> &callback)
{
	Object wrapped(args);

	rpc_call_t call = rpc_connection_call(m_connection, path.c_str(),
	    interface.c_str(), name.c_str(), wrapped.unwrap(),
	    ^bool(rpc_call_t c) {
		return (callback(Call::wrap(c)));
	});

	if (call == nullptr)
		throw (Exception::last_error());
}

void
Client::connect(const std::string &uri, const librpc::Object &params)
{
	m_client = rpc_client_create(uri.c_str(), params.unwrap());
	if (m_client == nullptr)
		throw (Exception::last_error());
}

void
Client::disconnect()
{
	if (m_client == nullptr)
		throw (Exception(ENOTCONN, "Not connected"));

	rpc_client_close(m_client);
	m_client = nullptr;
}

std::vector<RemoteInterface>
RemoteInstance::interfaces()
{

}

const std::string &
RemoteInstance::path()
{
	return (m_path);
}