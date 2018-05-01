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
#include <rpc/object.h>
#include <rpc/connection.h>
#include <rpc/client.h>
#include <rpc/service.h>

namespace librpc {
	class Exception: public std::runtime_error
	{
	public:
	    	Exception(int code, const std::string &message);
	    	static Exception last_error();

	    	int code();
	    	const std::string &message();

	private:
	    	int m_code;
	    	std::string m_message;
	};

	/**
	 *
	 */
	class Object
	{
	public:
		/**
		 *
		 * @param other
		 */
		Object(const Object &other);

		/**
		 *
		 * @param other
		 */
		Object(Object &&other) noexcept;

		/**
		 *
		 */
		Object();

		/**
		 *
		 * @param value
		 */
		Object(std::nullptr_t value);

		/**
		 *
		 * @param value
		 */
		Object(bool value);
		Object(uint64_t value);
		Object(int64_t value);
		Object(double value);
		Object(const char *value);
		Object(const std::string &value);
		Object(void *data, size_t len, rpc_binary_destructor_t dtor);
		Object(const std::map<std::string, Object> &dict);
		Object(const std::vector<Object> &array);
		Object(std::initializer_list<Object> list);
		Object(std::initializer_list<std::pair<std::string, Object>> list);
		virtual ~Object();

		static Object wrap(rpc_object_t other);
		rpc_type_t type();
	    	Object copy();
		void retain();
		void release();
		rpc_object_t unwrap() const;
	    	std::string describe();
	    	Object get(const std::string &key, const Object &def = Object(nullptr));
		Object get(size_t index, const Object &def = Object(nullptr));
		void push_back(const Object &value);
		void set(const std::string &key, const Object &value);
		int get_error_code();
		std::string get_error_message();

		explicit operator int64_t() const;
	 	explicit operator uint64_t() const;
		explicit operator int() const;
		operator const char *() const;
	    	operator std::string() const;
	    	Object operator[](const std::string &key);
	    	Object operator[](size_t index);

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

	class Client: public Connection
	{
	public:
		void connect(const std::string &uri, const Object &params);
		void disconnect();

	private:
		rpc_client_t m_client;
	};

	class RemoteInterface
	{
	public:
		Object call(const std::string &name, const std::vector<Object> &args);
		Object get(const std::string &prop);
		void set(const std::string &prop, Object value);
	};

	class RemoteInstance
	{
	public:
	    const std::string &path();
	    std::vector<RemoteInterface> interfaces();

	private:
	    rpc_connection_t m_connection;
	    std::string m_path;
	};


};

#endif /* LIBRPC_LIBRPC_HH */