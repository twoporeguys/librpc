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

#include <string.h>
#include <glib.h>
#include <libusb-1.0/libusb.h>
#include "../internal.h"
#include "../linker_set.h"

#define	LIBRPC_USB_VID		0xbeef
#define	LIBRPC_USB_PID		0xfeed

struct usb_context;
struct usb_connection;

static void *usb_open(void);
static void usb_close(void *);
static int usb_connect(struct rpc_connection *, const char *, rpc_object_t);
static int usb_send_msg(void *, void *, size_t, const int *fds, size_t nfds);
static int usb_abort(void *);
static int usb_get_fd(void *);
static int usb_ping(void *, const char *);
static int usb_enumerate(void *, struct rpc_bus_node **, size_t *);
static int usb_xfer(struct libusb_device *, int, void *, size_t, unsigned int);
static void goddamned_control_transfer_cb(struct libusb_transfer *);
static int goddamned_control_transfer(struct libusb_device *, int, uint8_t,
    uint8_t, uint16_t, uint16_t, void *, uint16_t, unsigned int);

enum librpc_usb_opcode
{
	LIBRPC_USB_PING = 0,
	LIBRPC_USB_READ_ACK,
	LIBRPC_USB_IDENTIFY,
	LIBRPC_USB_SEND_REQ,
	LIBRPC_USB_READ_RESP,
	LIBRPC_USB_READ_EVENTS,
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
	uint16_t 	start;
	uint16_t 	end;
	char 		buffer[];
} __attribute__((packed));

struct usb_context
{
	libusb_context *		uc_libusb;
	libusb_hotplug_callback_handle 	uc_handle;
};

struct usb_connection
{
	libusb_context *		uc_libusb;
	libusb_device *			uc_dev;
};

struct rpc_bus_transport libusb_bus_ops = {
	.open = usb_open,
	.close = usb_close,
	.enumerate = usb_enumerate,
	.ping = usb_ping
};

static const struct rpc_transport libusb_transport = {
	.name = "libusb",
	.schemas = {"libusb", NULL},
	.connect = usb_connect,
	.listen = NULL,
	.bus_ops = &libusb_bus_ops
};

static GMutex usb_mtx;
static GCond usb_cv;

static int
usb_hotplug_callback(libusb_context *ctx, libusb_device *dev,
    libusb_hotplug_event evt, void *arg)
{

	return (0);
}

static void *
usb_open(void)
{
	struct usb_context *ctx;
	int ret;

	ctx = g_malloc0(sizeof(*ctx));

	if (libusb_init(&ctx->uc_libusb) != 0) {
		g_free(ctx);
		return (NULL);
	}

	ret = libusb_hotplug_register_callback(ctx->uc_libusb,
	    LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
	    LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
	    LIBUSB_HOTPLUG_ENUMERATE, LIBRPC_USB_VID, LIBRPC_USB_PID,
	    LIBUSB_HOTPLUG_MATCH_ANY, &usb_hotplug_callback, ctx,
	    &ctx->uc_handle);

	if (ret != LIBUSB_SUCCESS) {
		libusb_exit(ctx->uc_libusb);
		g_free(ctx);
		return (NULL);
	}

	return (ctx);
}

static void
usb_close(void *arg)
{

}

static int
usb_connect(struct rpc_connection *rco, const char *uri_string,
    rpc_object_t args __unused)
{
	struct usb_connection *conn;

	conn = g_malloc0(sizeof(*conn));
	libusb_init(&conn->uc_libusb);

	rco->rco_send_msg = usb_send_msg;
	rco->rco_abort = usb_abort;
	rco->rco_get_fd = usb_get_fd;

	return (0);
}

static int
usb_send_msg(void *arg, void *buf, size_t len, const int *fds __unused,
    size_t nfds __unused)
{

}

static int
usb_abort(void *arg)
{

}

static int
usb_get_fd(void *arg)
{

	return (-1);
}

static int
usb_ping(void *arg, const char *name)
{
	struct usb_context *ctx = arg;
	struct {
		uint8_t setup[8];
		uint8_t status;
	} packet;

	if (usb_xfer(0, LIBRPC_USB_PING, &packet, sizeof(packet), 500) != 0)
		return (-1);

	return (packet.status == LIBRPC_USB_OK);
}

static int
usb_enumerate(void *arg, struct rpc_bus_node **resultp, size_t *countp)
{
	struct usb_context *ctx = arg;
	struct libusb_device_descriptor desc;
	libusb_device **devices;
	libusb_device *dev;

	libusb_get_device_list(ctx->uc_libusb, &devices);

	for (; *devices != NULL; devices++) {
		dev = *devices;
		libusb_get_device_descriptor(dev, &desc);
	}
}


static int
usb_xfer(struct libusb_device *handle, int opcode, void *buf, size_t len,
    unsigned int timeout)
{
	int ret;

	ret = goddamned_control_transfer(handle, 1,
	    LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN, (uint8_t)opcode,
	    0, 0, buf, (uint16_t)len, timeout);

	return (ret);
}

static libusb_device_handle *
usb_find_by_name(struct libusb_context *libusb, const char *name)
{
	libusb_device **devices;
	libusb_device *dev;

	libusb_get_device_list(libusb, &devices);

	for (; *devices != NULL; devices++) {
		dev = *devices;

	}
}

static void
goddamned_control_transfer_cb(struct libusb_transfer *xfer)
{
	struct usb_context *ctx = xfer->user_data;

	g_mutex_lock(&usb_mtx);
	g_cond_broadcast(&usb_cv);
	g_mutex_unlock(&usb_mtx);
}

static int
goddamned_control_transfer(struct libusb_device *handle, int ep,
    uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
    void *buf, uint16_t len, unsigned int timeout)
{
	struct libusb_transfer *xfer;

	g_mutex_lock(&usb_mtx);

	xfer = libusb_alloc_transfer(0);
	libusb_fill_control_setup(buf, request_type, bRequest, wValue,
				  wIndex, len);
	libusb_fill_control_transfer(xfer, handle, buf,
				     goddamned_control_transfer_cb, NULL, timeout);

	xfer->endpoint = (uint8_t)ep;
	libusb_submit_transfer(xfer);

	g_cond_wait(&usb_cv, &usb_mtx);
	g_mutex_unlock(&usb_mtx);

	return (xfer->status);
}

DECLARE_TRANSPORT(libusb_transport);
