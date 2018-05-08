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

struct b {
	char *	path;
	char *	interface;
} args [] = 
	{{NULL, NULL},
	 {"/", RPC_DEFAULT_INTERFACE}};
struct b base;

struct u {
	char *	scheme;
	char *	srv;
	char *	cli;
	bool 	good;
} uris[] =
	{{"tcp", "tcp://0.0.0.0:5000", "tcp://127.0.0.1:5000", true},
	 {"tpp", "tpp://0.0.0.0:5000", "tcp://127.0.0.1:5000", false}, 
	 {"tcp", "tcp://0.42.42.42:42", "tcp://127.0.0.1:5000", false}, 
	 {"unix", "unix://test.sock", "unix://test.sock", true},
	 {"unix", "unix:/", "unix://test.sock", false},
	 {"ws", "ws://0.0.0.0:6000/ws", "ws://127.0.0.1:6000/ws", true}, 
//	 {"ws", "ws://w0.0.0.0:6000/ws", "ws://127.0.0.1:6000/ws", false}, 
	 {"loopback", "loopback://0", "loopback://0", true},
	 {"loopback", "loopback://a", "loopback://0", false}, 
	 {0, "", "", 0}};

typedef struct {
	rpc_context_t	ctx;
        int 		iuri;
	int 		count;
	rpc_server_t 	srv;
	bool 		resume;
	int		iclose;
	int		threads;
	int		ok;
	int		called;
	int		woke;
	GRand *		rand;
	char *		str;
	int		close;
	int		abort;
} server_fixture;

static void
server_wait(rpc_context_t context, const char *uri)
{
	
	while (rpc_server_find(uri, context) != NULL) {
			sleep(5);
	}
}

static void
server_test_basic_set_up(server_fixture *fixture, gconstpointer user_data)
{

	fixture->ctx = rpc_context_create();
        fixture->iuri = (int)user_data;
	fixture->count = 0;
	fixture->srv = NULL;
}

static void
valid_server_set_up(server_fixture *fixture, gconstpointer u_data)
{

	fixture->ctx = rpc_context_create();
        fixture->iuri = (int)u_data;
	fixture->count = 0;
	fixture->iclose = 0;

        rpc_context_register_block(fixture->ctx, base.interface, "hi",
            NULL, ^(void *cookie __unused, rpc_object_t args) {
		fixture->count++;
                return rpc_string_create_with_format("hello %s!",
                    rpc_array_get_string(args, 0));
            });

        rpc_context_register_block(fixture->ctx, base.interface, "block",
            NULL, ^(void *cookie __unused, 
		rpc_object_t args __unused) {
		fixture->called++;
		sleep(fixture->called * 2);
		fixture->woke++;
                return (rpc_string_create("haha lol"));
            });

	fixture->srv = rpc_server_create(uris[fixture->iuri].srv, fixture->ctx);
}

static void
server_test_stream_setup(server_fixture *fix, gconstpointer u_data)
{
        GRand *rand = g_rand_new ();
        gint n = g_rand_int_range (rand, 1, STREAMS);
	__block server_fixture *fixture = fix;
	int res;

	base = args[0];
	valid_server_set_up(fixture, u_data);
	fixture->count = n;
	fixture->rand = rand;
	fixture->str = g_malloc(27);
	strcpy(fixture->str, "abcdefghijklmnopqrstuvwxyz");
	if (fixture->close > 0)
		fixture->close = g_rand_int_range (rand, 1, n);
	else if (fixture->abort > 0)
		fixture->abort = g_rand_int_range (rand, 1, n);
	
        res = rpc_context_register_block(fixture->ctx, base.interface, "stream",
            NULL, ^rpc_object_t (void *cookie, rpc_object_t args __unused) {
		int cnt = 0;	
		gint i;
		rpc_object_t res;

		while (cnt < fixture->count) {
			cnt++;
			if (cnt == fixture->close) {
				rpc_server_close(fixture->srv);
				return (rpc_null_create());
			} else if (cnt == fixture->abort) {
				rpc_function_kill(cookie);
				return (rpc_null_create());
			}
			i = g_rand_int_range (fixture->rand, 0, 26);

			res = rpc_object_pack("[s, i, i]", 
			    fixture->str + i, 26-i, cnt);
			if (rpc_function_yield(cookie, res) != 0) {
                        	rpc_function_end(cookie);
                        	return (rpc_null_create());
                	}
		}	
		rpc_function_end(cookie);
		return (rpc_null_create());
            });
	g_assert(res == 0);

}

static void
server_test_valid_server_set_up(server_fixture *fixture, gconstpointer u_data)
{

	base = args[0];
	valid_server_set_up(fixture, u_data);
}

static void
server_test_alt_server_set_up(server_fixture *fixture, gconstpointer u_data)
{

	base = args[1];
	valid_server_set_up(fixture, u_data);
}

static void
server_test_valid_server_tear_down(server_fixture *fixture, gconstpointer user_data)
{
	
	if (fixture->iclose == 0 && fixture->close == 0) {
		rpc_server_close(fixture->srv);
	}
	server_wait(fixture->ctx, uris[fixture->iuri].srv);
        rpc_context_unregister_member(fixture->ctx, NULL, "hi");
        rpc_context_unregister_member(fixture->ctx, NULL, "block");
	rpc_context_free(fixture->ctx);
}

static void
server_test_stream_tear_down(server_fixture *fixture, gconstpointer user_data)
{

	if (fixture->rand)
		g_rand_free(fixture->rand);
	if (fixture->str)
		g_free(fixture->str);
	rpc_context_unregister_member(fixture->ctx, NULL, "stream");
	server_test_valid_server_tear_down(fixture, user_data);
}

static void
server_test_basic_tear_down(server_fixture *fixture, gconstpointer user_data)
{

	rpc_context_free(fixture->ctx);
}

static gpointer
thread_stream_func (gpointer data)
{

	rpc_client_t client;
	rpc_connection_t conn;
	//rpc_object_t err;
	rpc_object_t result;
	rpc_call_t call;
	int cnt=0;
	int64_t num;
	int i;
	int64_t len;
	const char* str;

	client = rpc_client_create(data, 0);
	if (client == NULL) {
		g_thread_exit (GINT_TO_POINTER (0));
	}
	conn = rpc_client_get_connection(client);

	call = rpc_connection_call(conn, NULL, NULL, "stream", rpc_array_create(), NULL);
	if (call == NULL) {
		rpc_client_close(client);
		g_thread_exit (GINT_TO_POINTER (0));	
	}
        for (;;) {
                rpc_call_wait(call);

                switch (rpc_call_status(call)) {
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
                                goto done;

                        default:
                                g_assert_not_reached();
                }
        }

done:
	rpc_client_close(client);
	g_thread_exit (GINT_TO_POINTER (cnt));
	return (NULL);
}

static gpointer
thread_func (gpointer data)
{

	rpc_client_t client;
	rpc_connection_t conn;
	rpc_object_t result;

	client = rpc_client_create(data, 0);
	if (client == NULL) {
		g_thread_exit (GINT_TO_POINTER (1));
	}
	conn = rpc_client_get_connection(client);
	result = rpc_connection_call_simple(conn, "hi", "[s]", "world");
	if (result == NULL) {
		rpc_client_close(client);
		g_thread_exit (GINT_TO_POINTER (1));
	} else if (rpc_is_error(result)) {
		rpc_client_close(client);
		g_thread_exit (GINT_TO_POINTER (1));
	}

	rpc_client_close(client);
	g_thread_exit (GINT_TO_POINTER (0));
	return (NULL);
}

static gpointer
thread_func_delay (gpointer data)
{

	rpc_client_t client;
	rpc_connection_t conn;
	/*rpc_object_t err;*/
	rpc_object_t result;

	client = rpc_client_create(data, 0);
	if (client == NULL) {
		g_thread_exit (GINT_TO_POINTER (1));
	}

	conn = rpc_client_get_connection(client);
	result = rpc_connection_call_simple(conn, "block", RPC_NULL_FORMAT);

	if (result == NULL) {
		rpc_client_close(client);
		g_thread_exit (GINT_TO_POINTER (1));

	} else if (rpc_is_error(result)) {
		rpc_client_close(client);
		g_thread_exit (GINT_TO_POINTER (1));
	}

	rpc_client_close(client);
	g_thread_exit (GINT_TO_POINTER (0));
	return (NULL);
}

static int
thread_test(int n, gpointer(*t_func)(gpointer), server_fixture *fx )
{
	int ret = 0;
	GThread *threads[THREADS];

	for (int i = 0; i < n; i++)
		threads[i] = g_thread_new ("test", t_func, uris[fx->iuri].cli);

	if (fx->resume)
		rpc_server_resume(fx->srv);

	for (int i = 0; i < n; i++) {
		if (fx->iclose > 0 && i == fx->iclose) {
			rpc_server_close(fx->srv);
		}
		ret += (int)g_thread_join (threads[i]);
	}

	return (ret);
}

static void
server_test_stream_run(server_fixture *fixture, gconstpointer user_data)
{
	int ret;

	rpc_server_resume(fixture->srv);
	ret = thread_test(1, &thread_stream_func, fixture);
	g_assert(ret == fixture->count);
}

static void
server_test_stream_close(server_fixture *fixture, gconstpointer user_data)
{
	int ret;

	rpc_server_resume(fixture->srv);
	fixture->close = 1;
	ret = thread_test(1, &thread_stream_func, fixture);
}

static void
server_test_stream_abort(server_fixture *fixture, gconstpointer user_data)
{
	int ret;

	rpc_server_resume(fixture->srv);
	fixture->abort = 1;
	ret = thread_test(1, &thread_stream_func, fixture);
}

static void
server_test_flush(server_fixture *fixture, gconstpointer user_data)
{
	GRand *rand = g_rand_new ();
	gint n = g_rand_int_range (rand, 3, THREADS);
	gint m = g_rand_int_range (rand, 1, n);

	g_rand_free(rand);
	g_assert_cmpint(fixture->count, ==, 0);

	fixture->iclose = m;
	fixture->resume = true;
	thread_test(n, &thread_func_delay, fixture);

	server_wait(fixture->ctx, uris[fixture->iuri].srv);
}
	
static void
server_test_resume(server_fixture *fixture, gconstpointer user_data)
{
	GRand *rand = g_rand_new ();
	gint n = g_rand_int_range (rand, 3, THREADS);
	int ret;

	g_rand_free(rand);
	g_assert_cmpint(fixture->count, ==, 0);

	fixture->resume = true;
	ret = thread_test(n, &thread_func, fixture);

	g_assert_cmpint(fixture->count + ret, ==, n);

	fixture->count = 0;
	rpc_server_pause(fixture->srv);
	ret = thread_test(n, &thread_func, fixture);

	g_assert_cmpint(fixture->count + ret, ==, n);
}
	
static void
server_test_failed_listen(server_fixture *fixture, gconstpointer user_data)
{
	rpc_object_t err;

	fixture->srv = rpc_server_create(uris[fixture->iuri].srv, fixture->ctx);
	g_assert_null(fixture->srv);
	err = rpc_get_last_error();
	
	g_assert_cmpint(strlen(rpc_error_get_message(err)), >, 0);			
}

static void
server_test_nullables(server_fixture *fixture, gconstpointer user_data)
{
	rpc_client_t client;
	rpc_connection_t conn;
	rpc_object_t result;
	rpc_object_t call_args1;
	rpc_object_t call_args2;
	rpc_call_t call1;
	rpc_call_t call2;

	rpc_server_resume(fixture->srv);
        client = rpc_client_create(uris[fixture->iuri].cli, 0);
        if (client == NULL) {
                g_thread_exit (GINT_TO_POINTER (1));
        }
        conn = rpc_client_get_connection(client);
        result = rpc_connection_call_simple(conn, "hi", "[s]", "world");
	g_assert(result != NULL && !(rpc_is_error(result)));
	g_assert_cmpstr("hello world!", ==, rpc_string_get_string_ptr(result));

	rpc_release(result);

	call_args1 = rpc_object_pack("[s]", args[0].interface);
	call1 = rpc_connection_call(conn, args[0].path, RPC_INTROSPECTABLE_INTERFACE,
            "get_methods", call_args1, NULL);
	rpc_call_wait(call1);
	g_assert(!rpc_is_error(rpc_call_result(call1)));
	
	call_args2 = rpc_object_pack("[s]", args[1].interface);
	call2 = rpc_connection_call(conn, args[1].path, RPC_INTROSPECTABLE_INTERFACE,
            "get_methods", call_args2, NULL);
	rpc_call_wait(call2);

	g_assert(!rpc_is_error(rpc_call_result(call2)));

	g_assert(rpc_equal(rpc_call_result(call1), rpc_call_result(call2)));
	rpc_call_free(call1);
	rpc_call_free(call2);
	rpc_client_close(client);
}

/*
static void
server_test(server_fixture *fixture, gconstpointer user_data)
{

}
*/

static void
server_test_all_listen(void)
{
	//rpc_object_t err;
	rpc_context_t ctx;
	rpc_server_t srv;
	
	ctx = rpc_context_create();
	for (int i = 0; uris[i].scheme != NULL; i++) {
		/*fprintf(stderr, "CREATING %s\n", uris[i].srv);*/
		srv = rpc_server_create(uris[i].srv, ctx);
		g_assert((srv != NULL) == uris[i].good);
		if (srv != NULL) {
			/*fprintf(stderr, "i = %d, good\n", i);*/
			rpc_server_close(srv);
			/*if (rpc_server_find(uris[i].srv, ctx))
				fprintf(stderr, "%s NOT closed\n",
				    uris[i].srv);*/
			g_assert(rpc_server_find(uris[i].srv, ctx) == NULL);
		}
	}
	rpc_context_free(ctx);
}

static void
server_test_register()
{

	g_test_add("/server/resume/tcp", server_fixture, (void *)0,
	    server_test_valid_server_set_up, server_test_resume,
	    server_test_valid_server_tear_down);

	g_test_add("/server/resume/ws", server_fixture, (void *)5,
	    server_test_valid_server_set_up, server_test_resume,
	    server_test_valid_server_tear_down);

	g_test_add("/server/resume/loopback", server_fixture, (void *)6,
	    server_test_valid_server_set_up, server_test_resume,
	    server_test_valid_server_tear_down);

	g_test_add("/server/listen/fail", server_fixture, (void *)1,
	    server_test_basic_set_up, server_test_failed_listen,
	    server_test_basic_tear_down);

	g_test_add("/server/nullables/null", server_fixture, (void *)0,
	    server_test_valid_server_set_up, server_test_nullables,
	    server_test_valid_server_tear_down);

	g_test_add("/server/nullables/notnull", server_fixture, (void *)0,
	    server_test_alt_server_set_up, server_test_nullables,
	    server_test_valid_server_tear_down);

	g_test_add_func("/server/listen/all", server_test_all_listen);

	g_test_add("/server/flush/tcp", server_fixture, (void *)0,
	    server_test_valid_server_set_up, server_test_flush,
	    server_test_valid_server_tear_down);

	g_test_add("/server/flush/ws", server_fixture, (void *)5,
	    server_test_valid_server_set_up, server_test_flush,
	    server_test_valid_server_tear_down);

	g_test_add("/server/stream/one", server_fixture, (void *)0,
	    server_test_stream_setup, server_test_stream_run,
	    server_test_stream_tear_down);

	g_test_add("/server/stream/close", server_fixture, (void *)0,
	    server_test_stream_setup, server_test_stream_close,
	    server_test_stream_tear_down);

	g_test_add("/server/stream/abort", server_fixture, (void *)0,
	    server_test_stream_setup, server_test_stream_abort,
	    server_test_stream_tear_down);

	g_test_add("/server/flush/loopback", server_fixture, (void *)6,
	    server_test_valid_server_set_up, server_test_flush,
	    server_test_valid_server_tear_down);


}

static struct librpc_test server = {
    .name = "server",
    .register_f = &server_test_register
};

DECLARE_TEST(server);
