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

Object
Call::result() const
{
	return Object::wrap(rpc_call_result(m_call));
}

enum rpc_call_status
Call::status() const
{
	return (enum rpc_call_status)rpc_call_status(m_call);
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

	if (call == nullptr) {

	}

	return Call::wrap(call);
}

Object
Connection::call_sync(const std::string &name, const std::vector<Object> &args,
    const std::string &path, const std::string &interface)
{

}

Object
Connection::call_async(const std::vector<Object> &args)
{

}