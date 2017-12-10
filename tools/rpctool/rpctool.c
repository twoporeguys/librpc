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
#include <stdbool.h>
#include <glib.h>
#include <rpc/object.h>
#include <rpc/connection.h>
#include <rpc/client.h>
#include <rpc/service.h>
#include <rpc/serializer.h>

static int cmd_tree(int argc, const char *argv[]);
static int cmd_inspect(int argc, const char *argv[]);
static int cmd_call(int argc, const char *argv[]);
static int cmd_get(int argc, const char *argv[]);
static int cmd_set(int argc, const char *argv[]);

static const char *server;
static char **args;
static bool json;
static bool yaml;

static struct {
	const char *name;
	int (*fn)(int argc, const char *argv[]);
} commands[] = {
    { "tree", cmd_tree },
    { "inspect", cmd_inspect },
    { "call", cmd_call },
    { "get", cmd_get },
    { "set", cmd_set },
    { NULL}
};

static GOptionEntry options[] = {
    { "server", 's', 0, G_OPTION_ARG_STRING, &server, "Server URI", NULL },
    { "json", 'j', 0, G_OPTION_ARG_NONE, &json, "JSON output", NULL },
    { "yaml", 'y', 0, G_OPTION_ARG_NONE, &yaml, "YAML output", NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &args, "Command...", NULL },
    { NULL }
};

static rpc_connection_t
connect(void)
{
	rpc_client_t client;

	client = rpc_client_create(server, NULL);
	return (rpc_client_get_connection(client));
}

static int
cmd_tree(int argc, const char *argv[])
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
cmd_inspect(int argc, const char *argv[])
{

}

static int
cmd_call(int argc, const char *argv[])
{

}

static int
cmd_get(int argc, const char *argv[])
{

}

static int
cmd_set(int argc, const char *argv[])
{

}

int
main(int argc, const char *argv[])
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
		fprintf(stderr, "No command specified. Use \"rpctool -h\" to see help.\n");
		return (1);
	}

	nargs = g_strv_length(args);
	cmd = args[0];

	for (i = 0; commands[i].name != NULL; i++) {
		if (!g_strcmp0(commands[i].name, cmd))
			return (commands[i].fn(nargs - 1, args + 1));
	}

	fprintf(stderr, "Command %s not found, use \"rpctool -h\" to see help\n", cmd);
	return (1);
}