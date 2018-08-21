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
#include <rpc/typing.h>

#define USAGE_STRING							\
    "Available commands:\n"						\
    "  tree\n"								\
    "  inspect <path>\n"						\
    "  call <path> <interface> <method> [arguments]\n"			\
    "  get <path> <interface> <property>\n"				\
    "  set <path> <interface> <property> <value>\n"			\
    "  listen <path>\n"

static int cmd_tree(int argc, char *argv[]);
static int cmd_inspect(int argc, char *argv[]);
static int cmd_call(int argc, char *argv[]);
static int cmd_get(int argc, char *argv[]);
static int cmd_set(int argc, char *argv[]);
static int cmd_listen(int argc, char *argv[]);
static void  usage(GOptionContext *);

static const char *server;
static const char **idls;
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
	{ "idl", 'i', 0, G_OPTION_ARG_STRING_ARRAY, &idls, "IDL files to load", NULL },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &args, "", NULL },
	{ }
};

static rpc_connection_t
connect(void)
{
	rpc_client_t client;
	rpc_object_t error;
	const char **idl;

	if (server == NULL)
		server = getenv("LIBRPC_SERVER");

	if (server == NULL) {
		fprintf(stderr, "Server URI not provided\n");
		exit(1);
	}

	rpct_init(true);

	if (idls != NULL) {
		for (idl = idls; *idl != NULL; idl++)
			rpct_load_types_dir(*idl);
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
	rpc_object_t properties;

	properties = rpc_connection_call_syncp(conn, path, RPC_OBSERVABLE_INTERFACE,
	    "get_all", "[s]", interface);

	if (rpc_is_error(properties)) {
		fprintf(stderr, "Failed to read properties: %s\n",
		    rpc_error_get_message(properties));
		return (-1);
	}

	printf("  Properties:\n");

	rpc_array_apply(properties, ^(size_t idx, rpc_object_t prop) {
		rpc_object_t value;
		const char *name;
		char *str;

		rpc_object_unpack(prop, "{s,v}",
		    "name", &name,
		    "value", &value);

		str = rpc_copy_description(value);
		printf("    %s (= %s)\n", name, str);
		g_free(str);
		return ((bool)true);
	});

	return (0);
}

static int
inspect_interface(rpc_connection_t conn, const char *path, const char *interface)
{
	rpc_object_t methods;

	methods = rpc_connection_call_syncp(conn, path,
	    RPC_INTROSPECTABLE_INTERFACE, "get_methods", "[s]", interface);

	printf("  Methods:\n");

	rpc_array_apply(methods, ^(size_t idx, rpc_object_t value) {
		printf("    %s\n", rpc_string_get_string_ptr(value));
		return ((bool)true);
	});

	return (inspect_properties(conn, path, interface));
}

static int
cmd_tree(int argc, char *argv[])
{
	rpc_object_t tree;
	rpc_connection_t conn;
	int ret = 0;

	conn = connect();
	tree = rpc_connection_call_syncp(conn, argv[0],
	    RPC_DISCOVERABLE_INTERFACE, "get_instances", "[]");

	if (rpc_is_error(tree)) {
		ret = 1;
		fprintf(stderr, "Cannot get instances: %s\n",
		    rpc_error_get_message(tree));
		goto error;
	}

	rpc_array_sort(tree, ^(rpc_object_t o1, rpc_object_t o2) {
		return (g_strcmp0(
		    rpc_dictionary_get_string(o1, "path"),
		    rpc_dictionary_get_string(o2, "path")));
	});

	rpc_array_apply(tree, ^(size_t idx, rpc_object_t i) {
		const char *path;
		const char *descr;

		if (rpc_object_unpack(i, "{s,s}",
		    "path", &path,
		    "description", &descr) < 2)
			return ((bool)true);

		printf("%s (%s)\n", path, descr != NULL ? descr : "<none>");
		return ((bool)true);
	});

error:
	rpc_release(tree);
	return (ret);
}

static int
cmd_inspect(int argc, char *argv[])
{
	rpc_connection_t conn;
	rpc_object_t result;
	int ret = 0;

	if (argc < 1) {
		fprintf(stderr, "Not enough arguments provided\n");
		return (1);
	}

	conn = connect();
	result = rpc_connection_call_syncp(conn, argv[0],
	    RPC_INTROSPECTABLE_INTERFACE, "get_interfaces", "[]");

	if (rpc_is_error(result)) {
		ret = 1;
		fprintf(stderr, "Cannot inspect instance: %s\n",
		    rpc_error_get_message(result));
		goto error;
	}

	rpc_array_apply(result, ^bool(size_t idx, rpc_object_t value) {
		const char *interface;

		interface = rpc_string_get_string_ptr(value);
		printf("Interface %s:\n", interface);
		inspect_interface(conn, argv[0], interface);
		return (true);
	});

error:
	rpc_release(result);
	return (ret);
}

static int
cmd_call(int argc, char *argv[])
{
	rpc_connection_t conn;
	rpc_call_t call;
	rpc_object_t args;
	rpc_object_t error;

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
		fprintf(stderr, "Not enough arguments provided\n");
		return (1);
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
		fprintf(stderr, "Not enough arguments provided\n");
		return (1);
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
	__block GMutex mtx;
	rpc_connection_t conn;

	if (argc < 1) {
		fprintf(stderr, "Not enough arguments provided\n");
		return (1);
	}

	g_mutex_init(&mtx);
	conn = connect();
	rpc_connection_register_event_handler(conn, argv[0],
	    RPC_OBSERVABLE_INTERFACE, "changed",
	    ^(const char *p, const char *i, const char *n, rpc_object_t args) {
		const char *prop_interface;
		const char *prop_name;
		rpc_object_t value;

		if (rpc_object_unpack(args, "{s,s,v}",
		    "interface", &prop_interface,
		    "name", &prop_name,
		    "value", &value) < 3)
			return;

		g_mutex_lock(&mtx);
		printf("New value of property %s.%s: ", prop_interface, prop_name);
		g_mutex_unlock(&mtx);
		output(value);
	});

	pause();
	return (0);
}

static void
usage(GOptionContext *context)
{
	g_autofree char *help;

	help = g_option_context_get_help(context, true, NULL);
	fprintf(stderr, "%s", help);
}

int
main(int argc, char *argv[])
{
	GError *err = NULL;
	GOptionContext *context;
	const char *cmd;
	int nargs;
	size_t i;

	context = g_option_context_new("<COMMAND> [ARGUMENTS...] - interact with librpc server");
	g_option_context_set_description(context, USAGE_STRING);
	g_option_context_add_main_entries(context, options, NULL);
	if (!g_option_context_parse(context, &argc, &argv, &err)) {
		usage(context);
	}

	if (args == NULL) {
		usage(context);
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