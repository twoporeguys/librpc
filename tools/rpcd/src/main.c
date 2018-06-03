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

#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <glib.h>
#include <rpc/object.h>
#include <rpc/connection.h>
#include <rpc/service.h>
#include <rpc/client.h>
#include <rpc/server.h>
#include <rpc/serializer.h>

struct rpcd_service
{
	rpc_instance_t 	instance;
	const char *	name;
	const char *	description;
	const char *	uri;
	bool		needs_activation;
};

static rpc_object_t rpcd_register_service(void *, rpc_object_t);
static rpc_object_t rpcd_service_connect(void *, rpc_object_t);
static rpc_object_t rpcd_service_unregister(void *, rpc_object_t);
static rpc_object_t rpcd_service_get_name(void *);
static rpc_object_t rpcd_service_get_description(void *);

static rpc_context_t rpcd_context;
static int rpcd_nservers;
static rpc_server_t *rpcd_servers;
static GHashTable *rpcd_services;
static int rpcd_log_level;
static const char **rpcd_service_dirs;
static const char **rpcd_listen;

static const GOptionEntry rpcd_options[] = {
	{
		.long_name = "log-level",
		.short_name = 'v',
		.arg = G_OPTION_ARG_INT,
		.arg_data = &rpcd_log_level,
		.description = "Log level",
		.arg_description = "N"
	},
	{
		.long_name = "service-directory",
		.short_name = 'd',
		.arg = G_OPTION_ARG_STRING_ARRAY,
		.arg_data = &rpcd_service_dirs,
		.description = "Directory containing service definitions",
		.arg_description = "PATH"
	},
	{
		.long_name = "listen",
		.short_name = 'l',
		.arg = G_OPTION_ARG_STRING_ARRAY,
		.arg_data = &rpcd_listen,
		.description = "Listen address",
		.arg_description = "URI"
	},
	{ }
};

static const struct rpc_if_member rpcd_vtable[] = {
	RPC_METHOD(register_service, rpcd_register_service),
	RPC_MEMBER_END
};

static const struct rpc_if_member rpcd_service_vtable[] = {
	RPC_PROPERTY_RO(name, rpcd_service_get_name),
	RPC_PROPERTY_RO(description, rpcd_service_get_description),
	RPC_METHOD(connect, rpcd_service_connect),
	RPC_METHOD(unregister, rpcd_service_unregister),
	RPC_MEMBER_END
};

static rpc_object_t
rpcd_register_service(void *cookie, rpc_object_t args)
{
	struct rpcd_service *service;
	const char *uri, *name, *description;

	if (rpc_object_unpack(args, "[{s,s,s}]",
	    "uri", &uri,
	    "name", &name,
	    "description", &description) < 3) {
		rpc_function_error(cookie, EINVAL, "Invalid arguments passed");
		return (NULL);
	}

	service = g_malloc0(sizeof(*service));
	service->uri = g_strdup(uri);
	service->name = g_strdup(name);
	service->description = g_strdup(description);
	service->instance = rpc_instance_new(service, "/%s", name);
	if (service->instance == NULL) {
		rpc_function_error_ex(cookie, rpc_get_last_error());
		return (NULL);
	}

	rpc_instance_register_interface(service->instance,
	    "com.twoporeguys.rpcd.Service", rpcd_service_vtable, service);

	g_hash_table_insert(rpcd_services, g_strdup(service->name), service);
	rpc_context_register_instance(rpcd_context, service->instance);

	syslog(LOG_NOTICE, "Registered service %s", service->name);

	return (rpc_string_create(rpc_instance_get_path(service->instance)));
}

static rpc_object_t
rpcd_service_connect(void *cookie, rpc_object_t args)
{
	struct rpcd_service *service;
	rpc_client_t client;
	rpc_connection_t conn;
	int fd;

	service = rpc_function_get_arg(cookie);
	client = rpc_client_create(service->uri, NULL);
	if (client == NULL) {
		rpc_function_error_ex(cookie, rpc_get_last_error());
		return (NULL);
	}

	conn = rpc_client_get_connection(client);
	fd = rpc_connection_get_fd(conn);

	return (rpc_fd_create(fd));
}

static rpc_object_t
rpcd_service_unregister(void *cookie, rpc_object_t args)
{

}

static rpc_object_t
rpcd_service_get_name(void *cookie)
{
	struct rpcd_service *service;

	service = rpc_property_get_arg(cookie);
	return (rpc_string_create(service->name));
}

static rpc_object_t
rpcd_service_get_description(void *cookie)
{
	struct rpcd_service *service;

	service = rpc_property_get_arg(cookie);
	return (rpc_string_create(service->description));
}

static int
rpcd_load_service(const char *path)
{
	GError *err = NULL;
	struct rpcd_service *svc;
	char *contents;
	size_t len;
	rpc_auto_object_t descriptor;

	if (!g_file_get_contents(path, &contents, &len, &err))
		return (-1);

	descriptor = rpc_serializer_load("yaml", contents, len);
	if (descriptor == NULL)
		return (-1);

	svc = g_malloc0(sizeof(*svc));
	if (rpc_object_unpack(descriptor, "{s,s,s}",
	    "name", &svc->name,
	    "description", &svc->description,
	    "uri", &svc->uri) < 3) {
		g_free(svc);
		return (-1);
	}

	svc->instance = rpc_instance_new(svc, "/%s", svc->name);
	if (svc->instance == NULL) {

	}

	rpc_instance_register_interface(svc->instance,
	    "com.twoporeguys.rpcd.Service", rpcd_service_vtable, svc);

	return (0);
}

static int
rpcd_load_services(void)
{
	GDir *dir;
	GError *err = NULL;
	const char **dirname;
	const char *name;
	char *path;

	for (dirname = rpcd_service_dirs; *dirname != NULL; (*dirname)++) {
		syslog(LOG_NOTICE, "Loading services from %s", *dirname);
		dir = g_dir_open(*dirname, 0, &err);
		if (dir == NULL) {
			continue;
		}

		for (;;) {
			name = g_dir_read_name(dir);
			if (name == NULL)
				break;


			path = g_build_filename(*dirname, name, NULL);
			rpcd_load_service(path);
			g_free(path);
		}
	}
}

int
main(int argc, char *argv[])
{
	GError *err = NULL;
	GOptionContext *parser;
	int i;

	openlog("rpcd", LOG_PID, LOG_DAEMON);
	parser = g_option_context_new("");
	g_option_context_add_main_entries(parser, rpcd_options, NULL);

	if (!g_option_context_parse(parser, &argc, &argv, &err)) {
		g_printerr("Cannot parse options: %s\n", err->message);
		g_free(err);
		g_option_context_free(parser);
		return (1);
	}

	rpcd_services = g_hash_table_new(g_str_hash, g_str_equal);
	rpcd_context = rpc_context_create();
	rpcd_nservers = rpc_server_sd_listen(rpcd_context, &rpcd_servers);
	if (rpcd_nservers == 0) {
		syslog(LOG_EMERG, "No addresses to listen on, exiting");
		exit(EXIT_FAILURE);
	}

	rpc_instance_register_interface(rpc_context_get_root(rpcd_context),
	    "com.twoporeguys.rpcd.ServiceManager", rpcd_vtable, NULL);

	for (i = 0; i < rpcd_nservers; i++)
		rpc_server_resume(rpcd_servers[i]);

	if (rpcd_service_dirs != NULL)
		rpcd_load_services();

	syslog(LOG_NOTICE, "Started");
	pause();
	return (0);
}
