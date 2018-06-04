#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <getopt.h>
#include <time.h>
#include <string.h>
#include <dbus/dbus.h>
#include "dbus-strings.h"

static void
timespec_diff(struct timespec *start, struct timespec *stop,
    struct timespec *result)
{
	if ((stop->tv_nsec - start->tv_nsec) < 0) {
		result->tv_sec = stop->tv_sec - start->tv_sec - 1;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
	} else {
		result->tv_sec = stop->tv_sec - start->tv_sec;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec;
	}
}

static inline void
enforce_error(DBusError const *error)
{
	if (dbus_error_is_set(error)) {
		fprintf(stderr, "%s: %s\n", error->name, error->message);
		exit(EXIT_FAILURE);
	}
}

static int64_t
send_message(DBusConnection *conn)
{
	DBusError error;
	DBusMessage *message;
	static const char *str = "hello";
	void *buffer;
	int count;

	dbus_error_init(&error);

	message = dbus_message_new_method_call(ECHO_SERVICE_NAME,
	    ECHO_SERVICE_PATH, ECHO_SERVICE_INTERFACE,
	    ECHO_SERVICE_METHODNAME_ECHO);

	if (!message) {
		printf("Didn't allocate message\n");
		abort();
	}

	dbus_message_append_args(message, DBUS_TYPE_STRING, &str,
	    DBUS_TYPE_INVALID);

	DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn,
	    message, DBUS_TIMEOUT_INFINITE, &error);
	enforce_error(&error);

	dbus_message_unref(message);
	message = NULL;

	dbus_message_get_args(reply, &error, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
	    &buffer, &count, DBUS_TYPE_INVALID);
	enforce_error(&error);

	dbus_message_unref(reply);
	reply = NULL;

	return (count);
}


void
usage(const char *argv0)
{

	fprintf(stderr, "Usage: %s [-c CYCLES]\n", argv0);
	fprintf(stderr, "       %s -h\n", argv0);
}

int main(int argc, char *const argv[])
{
	DBusConnection *conn;
	DBusError error;
	struct timespec start;
	struct timespec end;
	struct timespec diff;
	struct timespec lat_start;
	struct timespec lat_end;
	double elapsed;
	double lat_sum = 0;
	bool quiet = false;
	int64_t bytes = 0;
	int64_t cycles;
	int64_t ncycles = 1000;
	int c;

	for (;;) {
		c = getopt(argc, argv, "c:hq");
		if (c == -1)
			break;

		switch (c) {
			case 'c':
				ncycles = strtoll(optarg, NULL, 10);
				break;

			case 'q':
				quiet = true;
				break;

			case 'h':
				usage(argv[0]);
				return (EXIT_SUCCESS);
		}
	}

	dbus_error_init(&error);

	conn = dbus_bus_get(DBUS_BUS_SESSION, &error);
	enforce_error(&error);

	clock_gettime(CLOCK_REALTIME, &start);

	for (cycles = 0; cycles < ncycles; cycles++) {
		clock_gettime(CLOCK_REALTIME, &lat_start);
		bytes += send_message(conn);
		clock_gettime(CLOCK_REALTIME, &lat_end);
		timespec_diff(&lat_start, &lat_end, &diff);
		lat_sum += diff.tv_sec + diff.tv_nsec / 1E9;
	}

	clock_gettime(CLOCK_REALTIME, &end);
	timespec_diff(&start, &end, &diff);
	elapsed = diff.tv_sec + diff.tv_nsec / 1E9;

	if (quiet) {
		printf("msgs=%" PRId64 " bytes=%" PRId64 " bps=%f pps=%f lat=%f\n",
		    cycles, bytes, bytes / elapsed, cycles / elapsed,
		    lat_sum / cycles);
	} else {
		printf("Received %" PRId64 " messages and %" PRId64 " bytes\n", cycles, bytes);
		printf("It took %.04f seconds\n", elapsed);
		printf("Average data rate: %.04f MB/s\n", bytes / elapsed / 1024 / 1024);
		printf("Average packet rate: %.04f packets/s\n", cycles / elapsed);
		printf("Average latency: %.08fs\n", lat_sum / cycles);
	}
	return (EXIT_SUCCESS);
}
