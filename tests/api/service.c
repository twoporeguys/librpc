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

#ifndef __unused
#define __unused __attribute__((unused))
#endif

typedef struct {
	rpc_context_t	context;
	int		c_servers;
	int		c_instances;
	int		port;
	volatile int	count;
	volatile int	door_cnt;
	volatile int	door;
} service_fixture;

static const struct rpc_if_member test_manager_vtable[] = {
	RPC_METHOD(test1, service_rpc_test1),
	RPC_PROPERTY_RO(counter1, service_get_counter),
	RPC_PROPERTY_RW(door, service_get_door, service_set_door),
	RPC_EVENT(service_door_changed),
	RPC_MEMBER_END
};

service_fixture fx_;

#define DOOR_CLOSED 0
#define DOOR_OPEN 1

static rpc_object_t
service_get_counter(void *cookie __unused)
{
	gint cnt = g_atomic_int_add(&fx_->counter, 1);

	return (rpc_int64_create(cnt));
}

static rpc_object_t
service_get_door(void *cookie __unused)
{

	return (rpc_int64_create(g_atomic_pointer_get(fx_->door)));
}

#define SERVICE_TEST_INTERFACE          "com.twoporeguys.ServiceTest"
static rpc_object_t
service_rpc_test1(void *cookie , rpc_object_t args)
{
	const int64 val;
	int64 op;
	const char *stat;
	rpc_instance_t instance = rpc_function_get_instance(cookie);

	g_assert(rpc_object_unpack(args, "[i,s]", &val, &stat) > 1);
	g_assert_cmpstr(stat, ==, (val == 0) ? "closed" : "open");
	
	op = (val + 1) &0x1;

	if (g_atomic_pointer_compare_and_exchange(&fx_->door, &op, &val) {
		/*changed!*/
		str = rpc_string_create(stat);
		rpc_instance_property_changed(instance, SERVICE_TEST_INTERFACE,
		    "door", stat);
	} else
		str = rpc_string_create("unchanged");

	return (str); 
}

static rpc_object_t
service_set_door(void *cookie , rpc_object_t args)
{
	const int64_t val;
	int ival;
	const char *stat;
	rpc_instance_t instance = rpc_function_get_instance(cookie);

	g_assert(rpc_object_unpack(args, "[i,s]", &val, &stat) > 1);
	g_assert_cmpstr(stat, ==, (val == 0) ? "closed" : "open");
	ival = (int)val;
	g_atomic_pointer_set(&fx->door, ival);
	return (NULL);  
}

static void
service_test(service_fixture *fixture, gconstpointer user_data)
{
	char buf[30];
	int i = 0;
	int port = user_data;
	rpc_server_t srv[3];

	for (;i < 3; i++) {
		sprintf(&buf, "tcp://0.0.0.0:%d", port + i);
		srv[i] = rpc_server_create(buf, fixture->context);
	}
	g_assert(rpc_context_free(fixture->context) == -1);
	for (i = 0; i < 3; i++) {
		rpc_server_close(srv[i]);
	}
	g_assert(rpc_context_free(fixture->context) == 0);
	fixture->close_ctx = false;
}

static int
service_register(service_fixture *fixture, gconstpointer user_data)
{

	rpc_instance_t inst = rpc_instance_new(NULL, "/STest");

	rpc_instance_register_interface(inst, SERVICE_TEST_INTERFACE,
	    service_test_vtable, NULL);

	/*NOTE IF arg == fixture, can we ditch fx_ ? */

	rpc_context_register_instance(fixture->context, inst);

	return (0);
}





static void
service_test_single_set_up(service_fixture *fixture, gconstpointer user_data)
{

	fixture->port = user_data;
	fixture_context = rpc_context_create();

	fixture->close_ctx = true;
	fixture->counter = 0;
	fixture->door = DOOR_CLOSED;

	g_assert_nonnull(fixture->context);
	fx_ = fixture;
}

static void
service_test_tear_down(service_fixture *fixture, gconstpointer user_data)
{

	if (fixture->close_ctx)
		rpc_context_free(fixture->context);
}

static void
service_test_register()
{

	g_test_add("/service/simple/servers", service_fixture, (void *)5400,
	    service_test_single_set_up, service_test,
	    service_test_tear_down);

	g_test_add("/service/simple/instances", service_fixture, (void *)5400,
	    service_test_single_set_up, service_instance_test,
	    service_test_tear_down);

}

static struct librpc_test service = {
    .name = "service",
    .register_f = &service_test_register
};

DECLARE_TEST(service);
