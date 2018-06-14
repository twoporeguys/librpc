/*+
 * Copyright 2017 Two Pore Guys, Inc.
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
#include <string.h>
#include <glib.h>
#include <libsoup/soup.h>
#include <libusb.h>
#include <rpc/object.h>
#include "../internal.h"
#include "../linker_set.h"

#ifndef NAME_MAX
#define NAME_MAX		255
#endif

#define	LIBRPC_USB_VID		0xbeef

struct usb_context;
struct usb_connection;

static void *usb_open(GMainContext *main_context);
static void usb_close(void *);
static int usb_hotplug_callback(libusb_context *, libusb_device *,
    libusb_hotplug_event, void *);
static gboolean usb_hotplug_impl(void *);
static bool usb_valid_pid(uint16_t);
static int usb_connect(struct rpc_connection *, const char *, rpc_object_t);
static int usb_send_msg(void *, void *, size_t, const int *fds, size_t nfds);
static int usb_abort(void *);
static int usb_get_fd(void *);
static int usb_ping(void *, const char *);
static int usb_enumerate_one(libusb_device *, struct rpc_bus_node *);
static int usb_enumerate(void *, struct rpc_bus_node **, size_t *);
static int usb_xfer(struct libusb_device_handle *, int, void *, size_t,
    unsigned int);
static libusb_device_handle *usb_find(struct libusb_context *, const char *,
    int);
static gboolean usb_send_msg_impl(void *);
static gboolean usb_event_impl(void *);
static void *usb_libusb_thread(void *);
static void usb_release(void *);

enum librpc_usb_opcode
{
	LIBRPC_USB_PING = 0,
	LIBRPC_USB_READ_ACK,
	LIBRPC_USB_IDENTIFY,
	LIBRPC_USB_SEND_REQ,
	LIBRPC_USB_READ_RESP,
	LIBRPC_USB_READ_EVENT,
	LIBRPC_USB_READ_LOG
};

enum librpc_usb_status
{
	LIBRPC_USB_OK = 0,
	LIBRPC_USB_ERROR,
	LIBRPC_USB_NOT_READY
};

struct librpc_usb_response
{
	uint8_t         status;
	char            data[];
};

struct librpc_usb_identification
{
	uint16_t        iName;
	uint16_t 	iDescription;
	uint16_t 	log_size;
};

struct librpc_usb_log
{
	uint8_t 	status;
	char 		buffer[];
};

struct usb_thread_state
{
	libusb_context *		uts_libusb;
	bool				uts_exit;
};

struct usb_send_state
{
	struct usb_connection *		uss_conn;
	void *				uss_buf;
	size_t 				uss_len;
};

struct usb_hotplug_state
{
	struct usb_context *		uss_ctx;
	libusb_device *			uss_dev;
	libusb_hotplug_event		uss_event;
};

struct usb_context
{
	libusb_context *		uc_libusb;
	libusb_hotplug_callback_handle 	uc_handle;
	GThread *			uc_thread;
	GMainContext *			uc_main_context;
	struct usb_thread_state		uc_state;
	GHashTable *			uc_devices;
};

struct usb_connection
{
	struct rpc_connection *		uc_rco;
	libusb_context *		uc_libusb;
	libusb_device *			uc_dev;
	libusb_device_handle *		uc_handle;
	GThread *			uc_libusb_thread;
	GSource *			uc_event_source;
	GMutex				uc_mtx;
	struct usb_thread_state		uc_state;
	size_t 				uc_logsize;
	int				uc_logfd;
};

struct rpc_bus_transport libusb_bus_ops = {
	.open = usb_open,
	.close = usb_close,
	.enumerate = usb_enumerate,
	.ping = usb_ping
};

static const struct rpc_transport libusb_transport = {
	.name = "libusb",
	.schemas = {"usb", "bus", NULL},
	.connect = usb_connect,
	.listen = NULL,
	.bus_ops = &libusb_bus_ops
};

static const uint16_t libusb_valid_pids[] = {
	0xfeed,
	0xfeee,
	0xbabe,
};

static GMutex usb_request_mtx;

static int
usb_hotplug_callback(libusb_context *ctx __unused, libusb_device *dev,
    libusb_hotplug_event evt, void *arg)
{
	struct usb_context *context = arg;
	struct usb_hotplug_state *state;

	state = g_malloc0(sizeof(*state));
	state->uss_ctx = context;
	state->uss_dev = dev;
	state->uss_event = evt;

	g_main_context_invoke(context->uc_main_context, usb_hotplug_impl,
	    state);
	return (0);
}

static gboolean
usb_hotplug_impl(void *arg)
{
	struct libusb_device_descriptor devdesc;
	struct usb_hotplug_state *state = arg;
	struct rpc_bus_node *node = NULL;
	uint32_t address;

	switch (state->uss_event) {
	case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED:
		node = g_malloc0(sizeof(struct rpc_bus_node));

		if (usb_enumerate_one(state->uss_dev, node) < 0) {
			g_free(node);
			break;
		}

		g_hash_table_insert(state->uss_ctx->uc_devices,
		    &node->rbn_address, node);
		rpc_bus_event(RPC_BUS_ATTACHED, node);
		break;

	case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT:
		if (libusb_get_device_descriptor(state->uss_dev, &devdesc) != 0)
			break;

		if (devdesc.idVendor != LIBRPC_USB_VID ||
		    !usb_valid_pid(devdesc.idProduct))
			break;

		address = libusb_get_device_address(state->uss_dev);
		node = g_hash_table_lookup(state->uss_ctx->uc_devices,
		    &address);

		if (node != NULL) {
			rpc_bus_event(RPC_BUS_DETACHED, node);
			g_hash_table_remove(state->uss_ctx->uc_devices,
			    &address);
			break;
		}

		node = g_malloc0(sizeof(struct rpc_bus_node));
		node->rbn_address = address;
		rpc_bus_event(RPC_BUS_DETACHED, node);
		g_free(node);
		break;

	default:
		g_assert_not_reached();
	}

	g_free(state);
	return (false);
}

static bool
usb_valid_pid(uint16_t pid)
{
	unsigned long i;

	for (i = 0; i < sizeof(libusb_valid_pids) / sizeof(uint16_t); i++) {
		if (libusb_valid_pids[i] == pid)
			return (true);
	}

	return (false);
}

static void
usb_free_node(struct rpc_bus_node *node)
{

	g_free((void *)node->rbn_name);
	g_free((void *)node->rbn_description);
	g_free((void *)node->rbn_serial);
	g_free(node);
}

static void *
usb_open(GMainContext *main_context)
{
	struct usb_context *ctx;
	int ret;

	ctx = g_malloc0(sizeof(*ctx));
	ctx->uc_main_context = main_context;
	ret = libusb_init(&ctx->uc_libusb);

	if (ret != 0) {
		rpc_set_last_error(ENXIO, "Failed to initialize libusb", NULL);
		g_free(ctx);
		return (NULL);
	}

	ret = libusb_hotplug_register_callback(ctx->uc_libusb,
	    LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
	    LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
	    LIBUSB_HOTPLUG_ENUMERATE, LIBRPC_USB_VID, LIBUSB_HOTPLUG_MATCH_ANY,
	    LIBUSB_HOTPLUG_MATCH_ANY, &usb_hotplug_callback, ctx,
	    &ctx->uc_handle);

	if (ret != LIBUSB_SUCCESS) {
		libusb_exit(ctx->uc_libusb);
		rpc_set_last_error(ENXIO, "Failed to initialize libusb", NULL);
		g_free(ctx);
		return (NULL);
	}

	ctx->uc_devices = g_hash_table_new_full(g_int_hash, g_int_equal, NULL,
	    (GDestroyNotify)usb_free_node);
	ctx->uc_thread = g_thread_new("libusb worker", usb_libusb_thread,
	    &ctx->uc_state);
	return (ctx);
}

static void
usb_close(void *arg)
{
	struct usb_context *ctx = arg;

	ctx->uc_state.uts_exit = true;
	libusb_hotplug_deregister_callback(ctx->uc_libusb, ctx->uc_handle);
	g_thread_join(ctx->uc_thread);
	g_hash_table_unref(ctx->uc_devices);
	libusb_exit(ctx->uc_libusb);
	g_free(ctx);
}

static int
usb_connect(struct rpc_connection *rco, const char *uri_string,
    rpc_object_t args)
{
	SoupURI *uri;
	struct usb_connection *conn;
	struct librpc_usb_identification ident;

	uri = soup_uri_new(uri_string);

	conn = g_malloc0(sizeof(*conn));
	conn->uc_logfd = -1;
	g_mutex_init(&conn->uc_mtx);
	libusb_init(&conn->uc_libusb);

	rpc_object_unpack(args, "f", &conn->uc_logfd);

	conn->uc_state.uts_libusb = conn->uc_libusb;
	conn->uc_state.uts_exit = false;
	conn->uc_libusb_thread = g_thread_new("libusb worker",
	    usb_libusb_thread, &conn->uc_state);

	if (uri->host != NULL)
		conn->uc_handle = usb_find(conn->uc_libusb, uri->host, -1);

	if (uri->port != 0)
		conn->uc_handle = usb_find(conn->uc_libusb, NULL, uri->port);

	soup_uri_free(uri);

	if (conn->uc_handle == NULL) {
		rpc_set_last_error(ENOENT, "Cannot find device", NULL);
		goto error;
	}

	/* Read the log buffer size */
	if (usb_xfer(conn->uc_handle, LIBRPC_USB_IDENTIFY, &ident,
	    sizeof(ident), 500) < 0) {
		rpc_set_last_error(EINVAL, "Cannot identify device", NULL);
		goto error;
	}

	conn->uc_event_source = g_timeout_source_new(500);
	g_source_set_callback(conn->uc_event_source, usb_event_impl, conn,
	    NULL);
	g_source_attach(conn->uc_event_source, rco->rco_main_context);

	conn->uc_logsize = ident.log_size;
	conn->uc_rco = rco;
	rco->rco_send_msg = usb_send_msg;
	rco->rco_abort = usb_abort;
	rco->rco_get_fd = usb_get_fd;
	rco->rco_arg = conn;
	rco->rco_release = usb_release;

	return (0);

error:
	conn->uc_state.uts_exit = true;
	g_thread_join(conn->uc_libusb_thread);

	libusb_close(conn->uc_handle);
	g_free(conn);
	return (-1);
}

static int
usb_send_msg(void *arg, void *buf, size_t len, const int *fds __unused,
    size_t nfds __unused)
{

	struct usb_connection *conn = arg;
	struct usb_send_state *send = g_malloc0(sizeof(*send));

	send->uss_buf = g_memdup(buf, (guint)len);
	send->uss_len = len;
	send->uss_conn = conn;
	g_main_context_invoke(conn->uc_rco->rco_main_context, usb_send_msg_impl,
	    send);
	return (0);
}

static int
usb_abort(void *arg)
{
	struct usb_connection *conn = arg;

	g_mutex_lock(&conn->uc_mtx);
	conn->uc_state.uts_exit = true;
	g_source_destroy(conn->uc_event_source);
	g_mutex_unlock(&conn->uc_mtx);
	libusb_close(conn->uc_handle);
	libusb_exit(conn->uc_libusb);
	g_thread_join(conn->uc_libusb_thread);
	return (0);
}

static int
usb_get_fd(void *arg __unused)
{

	return (-1);
}

static int
usb_ping(void *arg, const char *serial)
{
	libusb_device_handle *handle;
	struct usb_context *ctx = arg;
	uint8_t st;

	handle = usb_find(ctx->uc_libusb, serial, -1);
	if (handle == NULL) {
		errno = ENOENT;
		return (-1);
	}

	if (usb_xfer(handle, LIBRPC_USB_PING, &st, sizeof(st), 500) < 0) {
		libusb_close(handle);
		return (-1);
	}

	libusb_close(handle);
	return (st == LIBRPC_USB_OK);
}

static int
usb_enumerate_one(libusb_device *dev, struct rpc_bus_node *node)
{
	struct libusb_device_descriptor desc;
	struct librpc_usb_identification ident;
	libusb_device_handle *handle;
	char name[NAME_MAX];
	char descr[NAME_MAX];
	char serial[NAME_MAX];
	int ret = 0;

	if (libusb_get_device_descriptor(dev, &desc) < 0)
		return (-1);

	if (desc.idVendor != LIBRPC_USB_VID || !usb_valid_pid(desc.idProduct))
		return (-1);

	if (libusb_open(dev, &handle) != 0)
		return (-1);

	if (usb_xfer(handle, LIBRPC_USB_IDENTIFY, &ident,
	    sizeof(ident), 500) < 0) {
		ret = -1;
		goto done;
	}

	if (libusb_get_string_descriptor_ascii(handle, (uint8_t)ident.iName,
	    (uint8_t *)name, sizeof(name)) < 0) {
		ret = -1;
		goto done;
	}

	if (libusb_get_string_descriptor_ascii(handle,
	    (uint8_t)ident.iDescription, (uint8_t *)descr,
	    sizeof(descr)) < 0) {
		ret = -1;
		goto done;
	}

	if (libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber,
	    (uint8_t *)serial, sizeof(serial)) < 0) {
		ret = -1;
		goto done;
	}

	node->rbn_address = libusb_get_device_address(dev);
	node->rbn_name = g_strdup(name);
	node->rbn_description = g_strdup(descr);
	node->rbn_serial = g_strdup(serial);
done:
	libusb_close(handle);
	return (ret);
}

static int
usb_enumerate(void *arg, struct rpc_bus_node **resultp, size_t *countp)
{
	struct usb_context *ctx = arg;
	struct libusb_device_descriptor desc;
	libusb_device **devices;
	libusb_device *dev;
	struct rpc_bus_node node;

	*countp = 0;
	*resultp = NULL;
	libusb_get_device_list(ctx->uc_libusb, &devices);

	for (; *devices != NULL; devices++) {
		dev = *devices;
		libusb_get_device_descriptor(dev, &desc);

		debugf("trying device %d (vid=0x%04x, pid=0x%04x)",
		    libusb_get_device_address(dev), desc.idVendor,
		    desc.idProduct);

		if (usb_enumerate_one(dev, &node) < 0)
			continue;

		*resultp = g_realloc(*resultp,
		    (*countp + 1) * sizeof(struct rpc_bus_node));
		(*resultp)[*countp] = node;
		(*countp)++;
	}

	return (0);
}


static int
usb_xfer(struct libusb_device_handle *handle, int opcode, void *buf, size_t len,
    unsigned int timeout)
{
	int ret;

	g_mutex_lock(&usb_request_mtx);
	ret = libusb_control_transfer(handle,
	    LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN, (uint8_t)opcode,
	    0, 0, buf, (uint16_t)len, timeout);

	g_mutex_unlock(&usb_request_mtx);
	return (ret);
}

static libusb_device_handle *
usb_find(struct libusb_context *libusb, const char *serial, int addr)
{
	struct libusb_device_descriptor desc;
	libusb_device **devices;
	libusb_device *dev;
	libusb_device_handle *handle;
	uint8_t str[NAME_MAX];
	int address;

	libusb_get_device_list(libusb, &devices);

	for (; *devices != NULL; devices++) {
		dev = *devices;
		address = libusb_get_device_address(dev);
		libusb_get_device_descriptor(dev, &desc);

		debugf("trying device %d (vid=0x%04x, pid=0x%04x)",
		    address, desc.idVendor, desc.idProduct);

		if (desc.idVendor != LIBRPC_USB_VID ||
		    !usb_valid_pid(desc.idProduct))
			continue;

		if (addr != -1 && address == addr) {
			if (libusb_open(dev, &handle) != 0)
				continue;

			return (handle);
		}

		if (serial != NULL) {
			if (libusb_open(dev, &handle) != 0)
				continue;

			libusb_get_string_descriptor_ascii(handle,
			    desc.iSerialNumber, str, sizeof(str));

			if (g_strcmp0((const char *)str, serial) == 0)
				return (handle);

			libusb_close(handle);
			continue;
		}
	}

	return (NULL);
}

static gboolean
usb_send_msg_impl(void *arg)
{
	struct usb_send_state *state = arg;
	struct usb_connection *conn = state->uss_conn;
	int ret;
	struct {
		uint8_t status;
		uint8_t request[4096];
	} packet;

	g_mutex_lock(&usb_request_mtx);
	memcpy(&packet.status, state->uss_buf, state->uss_len);
	if (libusb_control_transfer(conn->uc_handle,
	    LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT,
	    LIBRPC_USB_SEND_REQ, 0, 0, (uint8_t *)&packet,
	    (uint16_t)state->uss_len, 500) < 0) {
		conn->uc_rco->rco_close(conn->uc_rco);
		goto out;
	}

	for (;;) {
		ret = libusb_control_transfer(conn->uc_handle,
		    LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN,
		    LIBRPC_USB_READ_RESP, 0, 0, (uint8_t *)&packet,
		    4095, 500);

		if (ret < 0) {
			debugf("failed to submit transfer, ret=%d", ret);
			continue;
		}

		switch (packet.status) {
		case LIBRPC_USB_OK:
			conn->uc_rco->rco_recv_msg(conn->uc_rco,
			    &packet.request, (size_t)ret, NULL,
			    0, NULL);
			goto out;

		case LIBRPC_USB_NOT_READY:
			g_usleep(1000 * 50); /* 50ms */
			break;

		case LIBRPC_USB_ERROR:
			conn->uc_rco->rco_close(conn->uc_rco);
			break;

		default:
			g_assert_not_reached();
			break;
		}
	}

out:
	g_mutex_unlock(&usb_request_mtx);
	g_free(state->uss_buf);
	g_free(state);
	return (false);
}

static gboolean
usb_event_impl(void *arg)
{
	struct usb_connection *conn = arg;
	int ret;
	struct librpc_usb_log *log;

	if (conn->uc_logfd == -1)
		return (false);

	log = g_malloc(sizeof(*log) + conn->uc_logsize);

	for (;;) {
		g_mutex_lock(&conn->uc_mtx);

		if (g_source_is_destroyed(conn->uc_event_source))
			break;

		memset(log, 0, sizeof(*log) + conn->uc_logsize);
		ret = usb_xfer(conn->uc_handle, LIBRPC_USB_READ_LOG, log,
		   sizeof(*log) + conn->uc_logsize, 500);

		g_mutex_unlock(&conn->uc_mtx);

		if (ret < 1)
			goto disconnected;

		if (log->status != LIBRPC_USB_OK)
			break;

		dprintf(conn->uc_logfd, "%*s", ret - 1, log->buffer);
		fsync(conn->uc_logfd);
	}

	g_free(log);
	return (!conn->uc_state.uts_exit);

disconnected:
	g_free(log);
	conn->uc_rco->rco_close(conn->uc_rco);
	return (false);
}

static void
usb_release(void *arg)
{

}

static void *
usb_libusb_thread(void *arg)
{
	struct usb_thread_state *uts = arg;

	while (!uts->uts_exit)
		libusb_handle_events(uts->uts_libusb);

	return (NULL);
}

DECLARE_TRANSPORT(libusb_transport);
