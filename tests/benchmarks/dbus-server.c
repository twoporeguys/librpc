/*+
 * Copyright 2018 Two Pore Guys, Inc.
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
#include <stdbool.h>
#include <dbus/dbus.h>
#include <unistd.h>
#include <string.h>
#include "dbus-strings.h"

static size_t msgsize = 4096;

static inline void
enforce_error(DBusError const *error)
{
	if (dbus_error_is_set(error)) {
		fprintf(stderr, "%s: %s\n", error->name, error->message);
		exit(EXIT_FAILURE);
	}
}

static void
handle_echo(DBusMessage *msg, DBusConnection *conn)
{
	DBusMessageIter args;
	DBusMessageIter sub;
	DBusMessage *reply;
	dbus_uint32_t serial = 0;
	char *param = NULL;
	void *buffer;

	if (!dbus_message_iter_init(msg, &args)) {
		fprintf(stderr, "Message has no arguments\n");
		abort();
	}

	if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) {
		fprintf(stderr, "Unexpcted argument type\n");
		abort();
	}

	buffer = malloc(msgsize);
	memset(buffer, 0x55, msgsize);

	dbus_message_iter_get_basic(&args, &param);
	reply = dbus_message_new_method_return(msg);

	dbus_message_iter_init_append(reply, &args);
	dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY,
	    DBUS_TYPE_BYTE_AS_STRING, &sub);
	dbus_message_iter_append_fixed_array(&sub, DBUS_TYPE_BYTE,
	    &buffer, msgsize);
	dbus_message_iter_close_container(&args, &sub);

	if (!dbus_connection_send(conn, reply, &serial)) {
		fprintf(stderr, "Failed to send reply\n");
		abort();
	}

	dbus_message_unref(reply);
	free(buffer);
}

static DBusHandlerResult
message_handler(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	if (dbus_message_is_method_call(msg, ECHO_SERVICE_INTERFACE,
	    ECHO_SERVICE_METHODNAME_ECHO)) {
		handle_echo(msg, conn);
		return (DBUS_HANDLER_RESULT_HANDLED);
	} else
		return (DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
}

void
usage(const char *argv0)
{

	fprintf(stderr, "Usage: %s [-s MSGSIZE]\n", argv0);
	fprintf(stderr, "       %s -h\n", argv0);
}

int
main(int argc, char *const argv[])
{
	DBusError error;
	int ret;
	int c;

	for (;;) {
		c = getopt(argc, argv, "u:s:mh");
		if (c == -1)
			break;

		switch (c) {
		case 's':
			msgsize = (size_t) strtol(optarg, NULL, 10);
			break;

		case 'h':
			usage(argv[0]);
			return (EXIT_SUCCESS);
		}
	}

	dbus_error_init(&error);

	DBusConnection *conn = dbus_bus_get(DBUS_BUS_SESSION, &error);
	enforce_error(&error);

	ret = dbus_bus_request_name(conn, ECHO_SERVICE_NAME, 0, &error);
	enforce_error(&error);

	if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		fprintf(stderr, "Didn't get name\n");
		return (EXIT_FAILURE);
	}

	if (!dbus_connection_add_filter(conn, message_handler, NULL, NULL)) {
		fprintf(stderr, "Couldn't install filter\n");
		return (EXIT_FAILURE);
	}

	printf("Using %zu message size\n", msgsize);

	while (dbus_connection_read_write_dispatch(conn,
	    DBUS_TIMEOUT_INFINITE));

	return (EXIT_SUCCESS);
}
