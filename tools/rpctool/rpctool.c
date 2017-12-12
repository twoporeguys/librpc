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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <glib.h>
#include <rpc/object.h>
#include <rpc/connection.h>
#include <rpc/client.h>
#include <rpc/service.h>
#include <rpc/serializer.h>

static int cmd_tree(int argc, char *argv[]);
static int cmd_inspect(int argc, char *argv[]);
static int cmd_call(int argc, char *argv[]);
static int cmd_get(int argc, char *argv[]);
static int cmd_set(int argc, char *argv[]);
static int cmd_listen(int argc, char *argv[]);

static const char *server;
static char **args;
static bool json;
static bool yaml;

static struct {
	const char *name;
	int (*fn)(int argc, char *argv[]);
} commands[] = {
	{ "tree", cmd_tree },
	{ "inspect", cmd_inspect },
	{ "call", cmd_call },
	{ "get", cmd_get },
	{ "set", cmd_set },
	{ "listen", cmd_listen },
	{ }
};

static GOptionEntry options[] = {
	{ "server", 's', 0, G_OPTION_ARG_STRING, &server, "Server URI", NULL },
	{ "json", 'j', 0, G_OPTION_ARG_NONE, &json, "JSON output", NULL },
	{ "yaml", 'y', 0, G_OPTION_ARG_NONE, &yaml, "YAML output", NULL },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &args, "", NULL },
	{ }
};

static rpc_connection_t
connect(void)
{
	rpc_client_t client;
	rpc_object_t error;

	if (server == NULL)
		server = getenv("LIBRPC_SERVER");

	if (server == NULL) {
		fprintf(stderr, "Server URI not provided\n");
		exit(1);
	}

	client = rpc_client_create(server, NULL);
	if (client == NULL) {
		error = rpc_get_last_error();
		fprintf(stderr, "Cannot connect: %s\n",
		    rpc_error_get_message(error));
		exit(1);
	}

	return (rpc_client_get_connection(client));
}

static void
output(rpc_object_t obj)
{
	char *str;
	void *frame;
	size_t len;

	if (obj == NULL)
		return;

	if (json) {
		if (rpc_serializer_dump("json", obj, &frame, &len) != 0)
			abort();

		printf("%s\n", (char *)frame);
		g_free(frame);
		return;
	}

	if (yaml) {
		if (rpc_serializer_dump("yaml", obj, &frame, &len) != 0)
			abort();

		printf("%s\n", (char *)frame);
		g_free(frame);
		return;
	}

	str = rpc_copy_description(obj);
	printf("%s\n", str);
	g_free(str);
}

static int
inspect_properties(rpc_connection_t conn, const char *path, const char *interface)
{
	rpc_call_t call;
	rpc_object_t args;
	rpc_object_t value;
	const char *name;
	char *str;
	int ret = 0;

	args = rpc_object_pack("[s]", interface);
	call = rpc_connection_call(conn, path, RPC_OBSERVABLE_INTERFACE,
	    "get_all", args, NULL);

	printf("  Properties:\n");

	for (;;) {
		rpc_call_wait(call);

		switch (rpc_call_status(call)) {
		case RPC_CALL_MORE_AVAILABLE:
			rpc_object_unpack(rpc_call_result(call), "{s,v}",
			    "name", &name,
			    "value", &value);

			str = rpc_copy_description(value);
			printf("    %s (= %s)\n", name, str);
			g_free(str);
			rpc_call_continue(call, false);
			break;

		case RPC_CALL_DONE:
			goto done;

		case RPC_CALL_ERROR:
			ret = 1;
			goto error;

		default:
			g_assert_not_reached();
		}
	}

	done:
	error:
	return (ret);
}

static int
inspect_interface(rpc_connection_t conn, const char *path, const char *interface)
{
	rpc_object_t args;
	rpc_call_t call;
	int ret = 0;

	args = rpc_object_pack("[s]", interface);
	call = rpc_connection_call(conn, path, RPC_INTROSPECTABLE_INTERFACE,
	    "get_methods", args, NULL);

	printf("  Methods:\n");

	for (;;) {
		rpc_call_wait(call);

		switch (rpc_call_status(call)) {
			case RPC_CALL_MORE_AVAILABLE:
				printf("    %s\n", rpc_string_get_string_ptr(
				    rpc_call_result(call)));
				rpc_call_continue(call, false);
				break;

			case RPC_CALL_DONE:
				goto done;

			case RPC_CALL_ERROR:
				ret = 1;
				goto error;

			default:
				g_assert_not_reached();
		}
	}

done:
	ret = inspect_properties(conn, path, interface);

error:
	return (ret);
}

static int
cmd_tree(int argc, char *argv[])
{
	rpc_call_t call;
	rpc_object_t tree;
	rpc_connection_t conn;
	int ret = 0;

	tree = rpc_array_create();
	conn = connect();
	call = rpc_connection_call(conn, "/", RPC_DISCOVERABLE_INTERFACE,
	    "get_instances", NULL, NULL);

	for (;;) {
		rpc_call_wait(call);

		switch (rpc_call_status(call)) {
		case RPC_CALL_MORE_AVAILABLE:
			rpc_array_append_stolen_value(tree, rpc_call_result(call));
			rpc_call_continue(call, false);
			break;

		case RPC_CALL_DONE:
			goto done;

		case RPC_CALL_ERROR:
			ret = 1;
			goto error;

		default:
			g_assert_not_reached();
		}
	}

done:
	rpc_array_sort(tree, ^(rpc_object_t o1, rpc_object_t o2) {
		return (g_strcmp0(
		    rpc_dictionary_get_string(o1, "path"),
		    rpc_dictionary_get_string(o2, "path")));
	});

	rpc_array_apply(tree, ^(size_t idx, rpc_object_t i) {
		const char *path;
		const char *descr;

		rpc_object_unpack(i, "{s,s}",
		    "path", &path,
		    "description", &descr);

		printf("%s (%s)\n", path, descr != NULL ? descr : "<none>");
		return ((bool)true);
	});

error:
	rpc_release(tree);
	rpc_call_free(call);
	return (ret);
}

static int
cmd_inspect(int argc, char *argv[])
{
	rpc_connection_t conn;
	rpc_call_t call;
	const char *interface;
	int ret = 0;

	conn = connect();
	call = rpc_connection_call(conn, argv[0], RPC_INTROSPECTABLE_INTERFACE,
	    "get_interfaces", NULL, NULL);

	for (;;) {
		rpc_call_wait(call);

		switch (rpc_call_status(call)) {
		case RPC_CALL_MORE_AVAILABLE:
			interface = rpc_string_get_string_ptr(rpc_call_result(call));
			printf("Interface %s:\n", interface);
			inspect_interface(conn, argv[0], interface);
			rpc_call_continue(call, false);
			break;

		case RPC_CALL_DONE:
			goto done;

		case RPC_CALL_ERROR:
			ret = 1;
			goto error;

		default:
			g_assert_not_reached();
		}
	}

done:
error:
	return (ret);
}

static int
cmd_call(int argc, char *argv[])
{
	rpc_connection_t conn;
	rpc_call_t call;
	rpc_object_t args;
	rpc_object_t error;
	int ret = 0;

	if (argc < 4) {
		fprintf(stderr, "Not enough arguments provided\n");
		return (1);
	}

	args = rpc_serializer_load("json", argv[3], strlen(argv[3]));
	if (args == NULL) {
		error = rpc_get_last_error();
		fprintf(stderr, "Cannot read input: %s\n",
		    rpc_error_get_message(error));
		return (1);
	}

	conn = connect();
	call = rpc_connection_call(conn, argv[0], argv[1], argv[2],
	    args, NULL);

	for (;;) {
		rpc_call_wait(call);

		switch (rpc_call_status(call)) {
			case RPC_CALL_MORE_AVAILABLE:
				output(rpc_call_result(call));
				rpc_call_continue(call, false);
				break;

			case RPC_CALL_DONE:
				output(rpc_call_result(call));
				goto done;

			case RPC_CALL_ERROR:
				ret = 1;
				goto error;

			default:
				g_assert_not_reached();
		}
	}

error:
	output(rpc_call_result(call));
done:
	rpc_call_free(call);
	return (0);
}

static int
cmd_get(int argc, char *argv[])
{
	rpc_connection_t conn;
	rpc_object_t result;

	if (argc != 3) {

	}

	conn = connect();
	result = rpc_connection_get_property(conn, argv[0], argv[1], argv[2]);
	output(result);

	return (rpc_is_error(result) ? 1 : 0);
}

static int
cmd_set(int argc, char *argv[])
{
	rpc_connection_t conn;
	rpc_object_t value;
	rpc_object_t result;

	if (argc != 4) {

	}

	value = rpc_serializer_load("json", argv[3], strlen(argv[3]));
	if (value == NULL) {
		output(rpc_get_last_error());
		return (1);
	}

	conn = connect();
	result = rpc_connection_set_property(conn, argv[0], argv[1], argv[2],
	    value);

	if (result != NULL && rpc_is_error(result))
		output(result);

	return (rpc_is_error(result) ? 1 : 0);
}

static int
cmd_listen(int argc, char *argv[])
{
	rpc_connection_t conn;

	conn = connect();
	rpc_connection_register_event_handler(conn, argv[0], argv[1], argv[2],
	    ^(const char *path, const char *interface, const char *name, rpc_object_t args) {
	    	output(args);
	    });

	pause();
	return (0);
}

int
main(int argc, char *argv[])
{
	GError *err = NULL;
	GOptionContext *context;
	const char *cmd;
	int nargs;
	size_t i;

	context = g_option_context_new(" - interact with librpc server");
	g_option_context_add_main_entries(context, options, NULL);
	if (!g_option_context_parse(context, &argc, &argv, &err)) {

	}

	if (args == NULL) {
		fprintf(stderr, "No command specified. Use \"rpctool -h\" to "
		    "get help.\n");
		return (1);
	}

	nargs = g_strv_length(args);
	cmd = args[0];

	for (i = 0; commands[i].name != NULL; i++) {
		if (!g_strcmp0(commands[i].name, cmd))
			return (commands[i].fn(nargs - 1, args + 1));
	}

	fprintf(stderr, "Command %s not found, use \"rpctool -h\" to "
	    "get help\n", cmd);
	return (1);
}