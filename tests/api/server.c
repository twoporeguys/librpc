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
#include <glib.h>
#include <stdio.h>
#include <rpc/object.h>
#include <rpc/service.h>
#include <rpc/server.h>
#include <rpc/client.h>

//static const int iuri_default = 0;

struct u {
	char *scheme;
	char *srv;
	char *cli;
	bool good;
	char *msg;
} uris[] =
	{{"tcp", "tcp://0.0.0.0:5000", "tcp://127.0.0.1:5000", true, ""},
	 {"tpp", "tpp://0.0.0.0:5000", "tcp://127.0.0.1:5000", false, 
		"Transport Not Found"},
	 {"tcp", "tcp://0.42.42.42:42", "tcp://127.0.0.1:5000", false, "binding"},
	 {"unix", "unix://test.sock", "unix://test.sock", true, ""},
	 {"loopback", "loopback://0", "loopback://0", true, ""},
	 {"loopback", "loopback://a", "loopback://0", false, "foo"},
	 {0, "", "", 0, ""}};

typedef struct {
	rpc_context_t ctx;
        int iuri;
	int count;
	rpc_server_t srv;
} server_fixture;

static void
server_test(server_fixture *fixture, gconstpointer user_data)
{

}

static void
server_test_single_set_up(server_fixture *fixture, gconstpointer user_data)
{

	fixture->ctx = rpc_context_create();
        fixture->iuri = (int)user_data;
	fixture->count = 0;
	fixture->srv = NULL;
}

static void
server_test_valid_server_set_up(server_fixture *fixture, gconstpointer user_data)
{

	fixture->ctx = rpc_context_create();
        fixture->iuri = (int)user_data;
	fixture->count = 0;

        rpc_context_register_block(fixture->ctx, NULL, "hi",
            NULL, ^(void *cookie __unused, rpc_object_t args) {
		fixture->count++;
                return rpc_string_create_with_format("hello %s!",
                    rpc_array_get_string(args, 0));
            });

        rpc_context_register_block(fixture->ctx, NULL, "block",
            NULL, ^(void *cookie __unused, rpc_object_t args __unused) {
		fixture->count++;
                return (rpc_string_create("haha lol"));
            });

	fixture->srv = rpc_server_create(uris[fixture->iuri].srv, fixture->ctx);
}

static void
server_test_valid_server_tear_down(server_fixture *fixture, gconstpointer user_data)
{

        rpc_context_unregister_member(fixture->ctx, NULL, "hi");
        rpc_context_unregister_member(fixture->ctx, NULL, "block");
	rpc_server_close(fixture->srv);
	rpc_context_free(fixture->ctx);
}

static void
server_test_single_tear_down(server_fixture *fixture, gconstpointer user_data)
{

	rpc_context_free(fixture->ctx);
}

#define THREADS 50

static gpointer
thread_func (gpointer data)
{

	rpc_client_t client;
	rpc_connection_t conn;
	rpc_object_t err;
	rpc_object_t result;

	client = rpc_client_create(data, 0);
	if (client == NULL) {
		fprintf(stderr, "CANT CREATE\n");
		g_thread_exit (GINT_TO_POINTER (1));
	}
	conn = rpc_client_get_connection(client);
	result = rpc_connection_call_simple(conn, "hi", "[s]", "world");
	if (result == NULL) {
		err = rpc_get_last_error();
                fprintf(stderr, "NULL RETURNED error: %s,code: %d\n",
		    rpc_error_get_message(err),
                    rpc_error_get_code(err)) ;
		g_thread_exit (GINT_TO_POINTER (1));
	} else if (rpc_is_error(result)) {
                fprintf(stderr, "THREAD CALL error: %s,code: %d\n",
		    rpc_error_get_message(result),
                    rpc_error_get_code(result));
		g_thread_exit (GINT_TO_POINTER (1));
	}

	rpc_client_close(client);
	g_thread_exit (GINT_TO_POINTER (0));
}

static void
server_test_resume(server_fixture *fixture, gconstpointer user_data)
{
	GThread *threads[THREADS];
	GRand *rand = g_rand_new ();
	gint n = g_rand_int_range (rand, 3, THREADS);
	int ret = 0;

	g_rand_free(rand);

	for (int i = 0; i < n; i++)
		threads[i] = g_thread_new ("test", thread_func, uris[0].cli);

	g_assert_cmpint(fixture->count, ==, 0);
	rpc_server_resume(fixture->srv);

	for (int i = 0; i < n; i++)
		ret += (int)g_thread_join (threads[i]);

	fprintf(stderr, "Received %d/%d calls post-resume, ret=%d\n", fixture->count, n, ret);
	g_assert_cmpint(fixture->count + ret, ==, n);
}
	
static void
server_test_register()
{

	g_test_add("/server/resume", server_fixture, (void *)0,
	    server_test_valid_server_set_up, server_test_resume,
	    server_test_valid_server_tear_down);

}

static struct librpc_test server = {
    .name = "server",
    .register_f = &server_test_register
};

DECLARE_TEST(server);
