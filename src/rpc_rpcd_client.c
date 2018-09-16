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

#include <errno.h>
#include <rpc/object.h>
#include <rpc/client.h>
#include <rpc/connection.h>
#include <rpc/rpcd.h>
#include "internal.h"

static const char *rpcd_get_socket_location(void);

static const char *
rpcd_get_socket_location(void)
{
	const char *location;

	location = getenv(RPCD_SOCKET_ENV);
	if (location == NULL)
		return (RPCD_SOCKET_LOCATION);

	return (location);
}

rpc_client_t
rpcd_connect_to(const char *rpcd_uri, const char *service_name)
{
	rpc_client_t client;
	rpc_connection_t conn;
	rpc_auto_object_t result = NULL;
	char *path;

	if (rpcd_uri == NULL)
		rpcd_uri = rpcd_get_socket_location();

	client = rpc_client_create(rpcd_uri, NULL);
	if (client == NULL)
		return (NULL);

	conn = rpc_client_get_connection(client);
	path = g_strdup_printf("/%s", service_name);
	result = rpc_connection_call_syncp(conn, path, RPCD_SERVICE_INTERFACE,
	    "connect", "[]");

	if (result == NULL) {
		rpc_client_close(client);
		return (NULL);
	}

	if (rpc_is_error(result)) {
		rpc_client_close(client);
		rpc_set_last_rpc_error(result);
		return (NULL);
	}

	if (rpc_get_type(result) == RPC_TYPE_FD) {
		/* File descriptor was passed to us */
		rpc_client_close(client);
		return (rpc_client_create("socket://", result));
	}

	if (g_strcmp0(rpc_string_get_string_ptr(result), "BRIDGED") == 0) {
		/* Bi-dir bridging */
		return (client);
	}

	g_assert_not_reached();
}

int
rpcd_services_apply(const char *rpcd_uri, rpcd_service_applier_t applier)
{
	rpc_client_t client;
	rpc_connection_t conn;
	rpc_auto_object_t result = NULL;
	if (rpcd_uri == NULL)
		rpcd_uri = rpcd_get_socket_location();

	client = rpc_client_create(rpcd_uri, NULL);
	if (client == NULL)
		return (-1);

	conn = rpc_client_get_connection(client);
	result = rpc_connection_call_syncp(conn, "/",
	    RPC_DISCOVERABLE_INTERFACE, "get_instances", "[]");

	rpc_array_apply(result, ^(size_t idx, rpc_object_t value) {
		const char *path;
		rpc_auto_object_t name;
		rpc_auto_object_t description;

		path = rpc_dictionary_get_string(value, "name");
		name = rpc_connection_get_property(conn, path,
		    RPCD_SERVICE_INTERFACE, "name");
		description = rpc_connection_get_property(conn, path,
		    RPCD_SERVICE_INTERFACE, "description");
		applier(rpc_string_get_string_ptr(name),
		    rpc_string_get_string_ptr(description));
		return ((bool)true);
	});

	return (0);
}

int
rpcd_register(const char *uri, const char *name, const char *description)
{
	rpc_client_t client;
	rpc_connection_t conn;
	rpc_auto_object_t result = NULL;

	client = rpc_client_create(rpcd_get_socket_location(), NULL);
	if (client == NULL)
		return (-1);

	conn = rpc_client_get_connection(client);
	result = rpc_connection_call_syncp(conn, "/", RPCD_MANAGER_INTERFACE,
	    "register_service",
	    "[<com.twoporeguys.librpc.rpcd.Service>{s,s,s}]",
	    "uri", uri,
	    "name", name,
	    "description", description);

	if (result == NULL) {
		rpc_client_close(client);
		return (-1);
	}

	if (rpc_is_error(result)) {
		rpc_client_close(client);
		rpc_set_last_rpc_error(result);
		return (-1);
	}

	return (0);
}

int
rpcd_unregister(const char *name)
{
	rpc_client_t client;
	rpc_connection_t conn;
	rpc_auto_object_t result = NULL;

	client = rpc_client_create(rpcd_get_socket_location(), NULL);
	if (client == NULL)
		return (-1);

	return (-1);
}
