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

#ifndef LIBRPC_LIBRPC_HH
#define LIBRPC_LIBRPC_HH

#include <string>
#include <vector>
#include <map>
#include <experimental/any>
#include <rpc/object.h>
#include <rpc/connection.h>
#include <rpc/service.h>

namespace librpc {
	typedef std::experimental::any any;

	class Exception: public std::runtime_error
	{
	public:
	    	Exception(int code, const std::string &message);
	};

	class Object
	{
	public:
		Object(const Object &other);
		Object(Object &&other);
		Object();
		Object(any value);
		Object(bool value);
		Object(uint64_t value);
		Object(int64_t value);
		Object(double value);
		Object(const char *value);
		Object(const std::string &value);
		Object(void *data, size_t len, rpc_binary_destructor_t dtor);
		Object(const std::map<std::string, any> &dict);
		Object(const std::map<std::string, Object> &dict);
		Object(const std::vector<any> &array);
		Object(const std::vector<Object> &array);
		Object(std::initializer_list<Object> list);
		Object(std::initializer_list<std::pair<std::string, Object>> list);

		static Object wrap(rpc_object_t other);
	    	Object copy();
		void retain();
		void release();
		rpc_object_t unwrap();
	    	std::string &&describe();

		explicit operator int64_t() const;
	 	explicit operator uint64_t() const;
		explicit operator int() const;
		explicit operator const char *() const;
	    	explicit operator std::string() const;

	private:
	    	rpc_object_t m_value;
	};

	class Call
	{
	public:
		Object result() const;
		enum rpc_call_status status() const;
	    	void resume(bool sync = false);
		void wait();
	    	void abort();

		static Call wrap(rpc_call_t other);

	private:
		Call() = default;
	    	rpc_call_t m_call;
	};

	class Connection
	{
	public:
		Call call(const std::string &name,
		    const std::vector<Object> &args,
		    const std::string &path = "/",
		    const std::string &interface = RPC_DEFAULT_INTERFACE);

		Object call_sync(const std::string &name,
		    const std::vector<Object> &args,
		    const std::string &path = "/",
		    const std::string &interface = RPC_DEFAULT_INTERFACE);

		Object call_async(const std::vector<Object> &args);

	private:
		rpc_connection_t m_connection;
	};
};

#endif /* LIBRPC_LIBRPC_HH */