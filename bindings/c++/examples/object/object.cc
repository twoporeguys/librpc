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

#include <string>
#include <map>
#include <iostream>
#include "../../include/librpc.hh"

int
main(int argc, const char *argv[])
{
	librpc::Object v("hello world");
	librpc::Object a = 2L;
	librpc::Object b(2L);
	librpc::Object list = {1L, 2L, 3UL};
	librpc::Object dict = {
	    {"foo", "bar"},
	    {"baz", -1L},
	    {"hello", true},
	    std::make_pair(
		"nested", librpc::Object {
		    std::make_pair("somekey", nullptr),
		    std::make_pair("somevalue", "b")
		}
	    )
	};

	std::cout << "v=" << v << std::endl;
	std::cout << "a+b=" << (int)a + (int)b << std::endl;
	std::cout << "list is " << list.describe() << std::endl;
	std::cout << "dict is " << dict.describe() << std::endl;
	std::cout << "dict[foo]=" << dict["foo"] << std::endl;
	std::cout << "dict[nested][somevalue]=" << dict["nested"]["somevalue"] << std::endl;

}