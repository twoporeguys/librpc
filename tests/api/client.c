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
/*
//#include "../catch.hpp"
#include <stdlib.h>
#include <errno.h>
#include <rpc/object.h>
#include <rpc/service.h>
#include <rpc/server.h>
#include <rpc/client.h>
#include "../src/internal.h"

static rpc_object_t
hello(void *cookie __unused, rpc_object_t args)
{

	return rpc_string_create_with_format("hello %s!",
	    rpc_array_get_string(args, 0));
}

SCENARIO("RPC_CLIENT", "Create a simple RPC server and connect to it") {
	GIVEN("A simple RPC server") {
		rpc_client_t client = NULL;
		rpc_connection_t conn;
		rpc_object_t result = NULL;
		rpc_context_t ctx = NULL;
		__block rpc_server_t srv = NULL;

		ctx = rpc_context_create();
		rpc_context_register_func(ctx, "hello", "Hello world function",
		    NULL, hello);

		srv = rpc_server_create("tcp://0.0.0.0:5000", ctx);

		client = rpc_client_create("tcp://127.0.0.1:5000", 0);
		conn = rpc_client_get_connection(client);

		WHEN("Client is connected") {
			THEN("Connection has been successfully established") {
				REQUIRE(client != NULL);
			}

			THEN("Connection is not NULL") {
				REQUIRE(conn != NULL);
			}

			AND_WHEN("Client calls an RPC method") {
				result = rpc_connection_call_sync(conn, "hello",
				    rpc_string_create("world"), NULL);

				THEN("Result is not NULL") {
					REQUIRE(result != NULL);
				}

				THEN("Results says: hello world!") {
					REQUIRE(!strcmp(
					    rpc_string_get_string_ptr(result),
					    "hello world!"));
				}

				rpc_release(result);
			}

			AND_WHEN("Client calls an RPC method again") {
				result = rpc_connection_call_sync(conn, "hello",
				    rpc_string_create("world"), NULL);

				THEN("Result is not NULL") {
					REQUIRE(result != NULL);
				}

				THEN("Results says: hello world!") {
					REQUIRE(!strcmp(
					    rpc_string_get_string_ptr(result),
					    "hello world!"));
				}

				rpc_release(result);
			}
		}

		if (client != NULL)
			rpc_client_close(client);

		if (srv != NULL)
			rpc_server_close(srv);

		if (ctx != NULL)
			rpc_context_free(ctx);
	}
}

*/

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

#include "../tests.h"
#include <errno.h>
#include "../../src/linker_set.h"
#include "../../src/internal.h"
#include <glib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <rpc/object.h>
#include <rpc/service.h>
#include <rpc/server.h>
#include <rpc/client.h>
#include <rpc/connection.h>

#define THREADS 50
#define STREAMS 50

struct u {
	char *  scheme;
	char *  srv;
	char *  cli;
	bool    good;
} uris_[] =
	{{"tcp", "tcp://0.0.0.0:5500", "tcp://127.0.0.1:5500", true},
	 {"tpp", "tpp://0.0.0.0:5500", "tcp://127.0.0.1:5500", false},
	 {"tcp", "tcp://0.42.42.42:42", "tcp://127.0.0.1:5500", false},
	 {"unix", "unix://test.sock", "unix://test.sock", true},
	 {"unix", "unix:/", "unix://test.sock", false},
	 {"ws", "ws://0.0.0.0:6600/ws", "ws://127.0.0.1:6600/ws", true},
	 {"ws", "ws://w0.0.0.0:6600/ws", "ws://127.0.0.1:6600/ws", false},
	 {"loopback", "loopback://0", "loopback://0", true},
	 {"loopback", "loopback://a", "loopback://0", false},
	 {0, "", "", 0}};


typedef struct {
	rpc_context_t		ctx;
	int			iuri;
	rpc_server_t		srv;
	rpc_connection_t 	conn;
	bool			resume;
	int			iclose;
	volatile int		count;
	char *			str;
	GRand *			rand;
	GThreadPool *		workers;
} client_fixture;

struct work_item {
	rpc_connection_t	conn;
	rpc_call_t		call;
	rpc_object_t		args;
	rpc_object_t		result;
	char *			method;
	int 			ccnt;
	client_fixture *	fx;
};

static gpointer
thread_func (gpointer data)
{

	rpc_client_t client;
	rpc_connection_t conn;
	rpc_object_t result;

	client = rpc_client_create(data, 0);
	if (client == NULL)
		g_thread_exit (GINT_TO_POINTER (1));

	conn = rpc_client_get_connection(client);
	result = rpc_connection_call_simple(conn, "hi", "[s]", "world");
	if (result == NULL) {
		rpc_client_close(client);
		g_thread_exit (GINT_TO_POINTER (1));
	} else if (rpc_is_error(result)) {
		rpc_client_close(client);
		g_thread_exit (GINT_TO_POINTER (1));
	} else
		g_assert_cmpstr(rpc_string_get_string_ptr(result), ==,
		    "hello world!");

	rpc_client_close(client);
	g_thread_exit (GINT_TO_POINTER (0));
	return (NULL);
}

static gpointer
thread_callme_func (gpointer data)
{

	rpc_client_t client;
	rpc_connection_t conn;
	rpc_context_t ctx = NULL;
	rpc_object_t result;

	client = rpc_client_create(data, 0);
	if (client == NULL)
		g_thread_exit (GINT_TO_POINTER (1));

	conn = rpc_client_get_connection(client);
	ctx = rpc_context_create();
	g_assert(rpc_connection_set_context(conn, ctx) == 0);

	rpc_context_register_block(ctx, NULL, "callme", NULL,
	    ^(void *cookie __unused, rpc_object_t args) {
		return (rpc_string_create("world!"));
	});


	result = rpc_connection_call_simple(conn, "callyou", RPC_NULL_FORMAT);
	g_assert(result != NULL);
	g_assert(!rpc_is_error(result));
	g_assert_cmpstr(rpc_string_get_string_ptr(result), ==,
	    "hello world!");

	rpc_client_close(client);
	rpc_context_unregister_member(ctx, NULL, "callme");
	if (ctx != NULL)
		rpc_context_free(ctx);
	g_thread_exit (GINT_TO_POINTER (0));

	return (NULL);
}

static gpointer
thread_mestream_func (gpointer data)
{

	rpc_client_t client;
	rpc_connection_t conn;
	rpc_context_t ctx = NULL;
	rpc_object_t result;
	rpc_object_t value;
	GRand *rand = g_rand_new ();
	gint n = g_rand_int_range (rand, 1, STREAMS);
	char *str = g_malloc(27);
	__block int cnt = 0;
	rpc_call_t call;

	client = rpc_client_create(data, 0);
	if (client == NULL)
		g_thread_exit (GINT_TO_POINTER (1));

	conn = rpc_client_get_connection(client);
	ctx = rpc_context_create();
	g_assert(rpc_connection_set_context(conn, ctx) == 0);

	strcpy(str, "abcdefghijklmnopqrstuvwxyz");

	rpc_context_register_block(ctx, NULL, "callme", NULL,
	    ^rpc_object_t(void *cookie, rpc_object_t args __unused) {
		gint i;
		rpc_object_t res;

		rpc_function_start_stream(cookie);
		while (cnt < n) {
			cnt++;
			i = g_rand_int_range (rand, 0, 26);

			res = rpc_object_pack("[s, i, i]",
			    str + i, (int64_t)26-i, (int64_t)cnt);
			if (rpc_function_yield(cookie, res) != 0)
				return(false);
		}
		rpc_function_end(cookie);
		return (RPC_FUNCTION_STILL_RUNNING);
	});

	value = rpc_object_pack("[s,i]", "callme", (int64_t)n);
	call = rpc_connection_call(conn, NULL, NULL, "stream-me", value, NULL);
	if (call == NULL)
		g_thread_exit (GINT_TO_POINTER (1));
	rpc_call_wait(call);
	result = rpc_call_result(call);

	g_assert(result != NULL);
	g_assert(!rpc_is_error(result));
	g_assert_cmpstr(rpc_string_get_string_ptr(result), ==,
	    "DONE!");

	rpc_call_free(call);
	rpc_client_close(client);
	rpc_context_unregister_member(ctx, NULL, "callme");
	if (ctx != NULL)
		rpc_context_free(ctx);
	g_thread_exit (GINT_TO_POINTER (0));

	return (NULL);
}

static int
thread_test(int n, gpointer(*t_func)(gpointer), client_fixture *fx )
{
	int ret = 0;
	GThread *threads[THREADS];

	for (int i = 0; i < n; i++)
		threads[i] = g_thread_new ("test", t_func, uris_[fx->iuri].cli);

	if (fx->resume)
		rpc_server_resume(fx->srv);

	for (int i = 0; i < n; i++) {
		if (fx->iclose > 0 && i == fx->iclose)
			rpc_server_close(fx->srv);
		ret += (int)g_thread_join (threads[i]);
	}

	return (ret);
}


static void
client_test(client_fixture *fixture, gconstpointer user_data)
{

	int ret = 0;

	fixture->resume = true;
	ret = thread_test(1, &thread_func, fixture);
	g_assert_cmpint(fixture->count + ret, ==, 1);
}

static void
client_server_calls_test(client_fixture *fixture, gconstpointer user_data)
{

	int ret = 0;

	fixture->resume = true;
	rpc_context_register_block(fixture->ctx, NULL, "callyou",	NULL,
	    ^rpc_object_t (void *cookie, rpc_object_t args __unused) {
		rpc_object_t result;
		#define EINVAL 22

		rpc_connection_t conn = rpc_function_get_connection(cookie);
		if (conn == NULL) {
			rpc_function_error(cookie, EINVAL, "No connection provided");
			return (NULL);
		}
		result = rpc_connection_call_simple(conn, "callme", RPC_NULL_FORMAT);
		g_assert_cmpstr(rpc_string_get_string_ptr(result), ==,
		    "world!");
		return rpc_string_create_with_format("hello %s",
			rpc_string_get_string_ptr(result));
	});

	ret = thread_test(1, &thread_callme_func, fixture);
	rpc_context_unregister_member(fixture->ctx, NULL, "callyou");
}

static int
do_stream_work(struct work_item *item)
{

	int cnt = 0;
	rpc_object_t result;
	const char *method;
	rpc_call_t call;
	int i;
	const char *str;
	int64_t len;
	int64_t num;
	bool failed = false;

	g_atomic_int_inc(&item->fx->count);
	i = rpc_object_unpack(item->args, "[s, i]", &method, &item->ccnt);
	g_assert(i == 2);

	call = rpc_connection_call(item->conn, NULL, NULL, method, rpc_array_create(), NULL);
	if (call == NULL) {
		rpc_function_error(item->call, EINVAL, "Unable to make call");
		return (0);
	}
	for (;;) {
		rpc_call_wait(call);

		switch (rpc_call_status(call)) {
		case RPC_CALL_STREAM_START:
			rpc_call_continue(call, false);
			break;

		case RPC_CALL_MORE_AVAILABLE:
			result = rpc_call_result(call);
			i = rpc_object_unpack(result,
			    "[s, i, i]", &str, &len, &num);
			cnt++;
			g_assert(i == 3);
			g_assert(len == (int)strlen(str));
			rpc_call_continue(call, false);
			break;

		case RPC_CALL_DONE:
		case RPC_CALL_ENDED:
			goto done;

		case RPC_CALL_ERROR:
			debugf("Client call error: %s %d",
			    rpc_error_get_message(rpc_call_result(call)),
			    rpc_error_get_code(rpc_call_result(call)));
			failed = true;
			goto done;

		default:
			g_assert_not_reached();
		}
	}
done:

	if (!failed)
		item->result = rpc_string_create("DONE!");
	else
		item->result = rpc_error_create(EPROTO,
		    "Error in receiving streamed data", NULL);
	rpc_call_free(call);
	return (cnt);
}

static void
stream_worker(void *arg, void *data)
{
	int cnt;
	struct work_item *item = arg;

	item->fx = data;
	cnt = do_stream_work(item);
	g_assert(cnt == item->ccnt);
	rpc_function_respond(item->call, item->result);
	rpc_function_release(item->call);
	g_free(item);
}

static void
client_multi_streams_test(client_fixture *fixture, gconstpointer user_data)
{
	int ret = 0;
	GError *err = NULL;
	GRand *rand = g_rand_new ();
	gint n = g_rand_int_range (rand, 3, THREADS);

	fixture->resume = true;
	fixture->workers = g_thread_pool_new(stream_worker, fixture,
	    g_get_num_processors() * 4, false, &err);
	g_assert_null(err);

	rpc_context_register_block(fixture->ctx, NULL, "stream-me",	NULL,
	    ^rpc_object_t (void *cookie, rpc_object_t args) {
		struct work_item *item;
		GError *error = NULL;

		rpc_function_retain(cookie);
		item = g_malloc0(sizeof(*item));
		item->call = cookie;
		item->args = args;
		item->conn = rpc_function_get_connection(cookie);
		g_thread_pool_push(fixture->workers, item, &error);
		g_assert_null(error);

		return (RPC_FUNCTION_STILL_RUNNING);
	});

	ret = thread_test(n, &thread_mestream_func, fixture);
	g_assert(fixture->count + ret == n);
	rpc_context_unregister_member(fixture->ctx, NULL, "stream-me");
}

static void
client_test_single_set_up(client_fixture *fixture, gconstpointer user_data)
{

	fixture->ctx = rpc_context_create();
	fixture->iuri = (int)user_data;

	rpc_context_register_block(fixture->ctx, NULL, "hi",
	    NULL, ^(void *cookie __unused, rpc_object_t args) {
		g_atomic_int_inc(&fixture->count);
		return rpc_string_create_with_format("hello %s!",
		    rpc_array_get_string(args, 0));
	    });

	fixture->srv = rpc_server_create(uris_[fixture->iuri].srv, fixture->ctx);
}

static void
client_test_tear_down(client_fixture *fixture, gconstpointer user_data)
{

	if (fixture->srv) {
		rpc_server_close(fixture->srv);
		while (rpc_server_find(uris_[fixture->iuri].srv, fixture->ctx) != NULL)
			sleep(5);
	}
	rpc_context_unregister_member(fixture->ctx, NULL, "hi");
	rpc_context_free(fixture->ctx);
}


static void
client_test_register()
{

	g_test_add("/client/simple/tcp", client_fixture, (void *)0,
	    client_test_single_set_up, client_test,
	    client_test_tear_down);

	g_test_add("/client/server-call/tcp", client_fixture, (void *)0,
	    client_test_single_set_up, client_server_calls_test,
	    client_test_tear_down);

	g_test_add("/client/multi-streams/tcp", client_fixture, (void *)0,
	    client_test_single_set_up, client_multi_streams_test,
	    client_test_tear_down);

}

static struct librpc_test client = {
    .name = "client",
    .register_f = &client_test_register
};

DECLARE_TEST(client);
