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

#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <glib.h>
#include <libsoup/soup.h>
#include <libudev.h>
#include <linux/netlink.h>
#include <linux/connector.h>
#include <rpc/bus.h>
#include "../linker_set.h"
#include "../internal.h"
#include "../../kmod/librpc.h"

#define	BUS_NL_MSGSIZE	65536

struct bus_netlink;
struct bus_connection;

static int bus_connect(struct rpc_connection *, const char *, rpc_object_t);
static int bus_send_msg(void *, void *, size_t, const int *, size_t);
static int bus_ping(const char *);
static int bus_enumerate(struct rpc_bus_node **, size_t *);
static int bus_abort(void *);
static int bus_get_fd(void *);
static int bus_netlink_open(struct bus_netlink *);
static int bus_netlink_send(struct bus_netlink *, struct librpc_message *,
    void *, size_t);
static int bus_netlink_recv(struct bus_netlink *, struct librpc_message *,
    void **, size_t *);
static int bus_lookup_address(const char *);
static void *bus_reader(void *);

struct rpc_transport bus_transport = {
	.name = "bus",
	.schemas = {"bus", "usb", NULL},
	.connect = bus_connect,
    	.ping = bus_ping,
    	.enumerate = bus_enumerate,
	.listen = NULL
};

struct bus_netlink
{
    	int 			bn_sock;
    	uint32_t 		bn_seq;
};

struct bus_connection
{
    	const char *		bc_name;
    	uint32_t		bc_address;
    	struct bus_netlink	bc_bn;
    	struct rpc_connection *	bc_parent;
    	GThread *		bc_thread;
};

static int
bus_connect(struct rpc_connection *rco, const char *uri_string,
    rpc_object_t args __unused)
{
	SoupURI *uri;
	struct bus_connection *conn;
	int ret;

	uri = soup_uri_new(uri_string);
	conn->bc_address = bus_lookup_address(uri->host);
	conn = g_malloc0(sizeof(struct bus_connection));
	conn->bc_parent = rco;

	if (bus_netlink_open(&conn->bc_bn) != 0) {
		g_free(conn);
		return (-1);
	}

	conn->bc_thread = g_thread_new("bus reader", &bus_reader, conn);

	rco->rco_send_msg = &bus_send_msg;
	rco->rco_abort = &bus_abort;
	rco->rco_arg = conn;
	return (0);
}


static int
bus_abort(void *arg)
{
	struct bus_connection *conn = arg;
}

static int
bus_get_fd(void *arg)
{
	struct bus_connection *conn = arg;

	return (conn->bc_bn.bn_sock);
}

static int
bus_ping(const char *name)
{
	struct bus_netlink bn;
	struct librpc_message msg;
	uint32_t address;

	if (bus_netlink_open(&bn) != 0)
		return (-1);

	address = bus_lookup_address(name);
	if (address == -1) {
		close(bn.bn_sock);
		return (-1);
	}

	msg.opcode = LIBRPC_PING;
	msg.address = address;
	msg.status = 0;

	return (bus_netlink_send(&bn, &msg, NULL, 0));
}

static int
bus_enumerate(struct rpc_bus_node **resultp, size_t *countp)
{
	struct rpc_bus_node *result = NULL;
	struct udev *udev;
	struct udev_enumerate *iter;
	struct udev_list_entry *entry, *list;
	struct udev_device *dev;
	size_t count = 0;

	udev = udev_new();
	iter = udev_enumerate_new(udev);

	udev_enumerate_add_match_subsystem(iter, "librpc");
	udev_enumerate_scan_devices(iter);

	list = udev_enumerate_get_list_entry(iter);

	udev_list_entry_foreach(entry, list) {
		const char *path = udev_list_entry_get_name(entry);

		dev = udev_device_new_from_syspath(udev, path);
		result = g_realloc(result, count + 1);
		result[count].rbn_name = g_strdup(
		    udev_device_get_property_value(dev, "name"));
		result[count].rbn_description = g_strdup(
		    udev_device_get_property_value(dev, "description"));
		count++;
	}

	*resultp = result;
	*countp = count;
	return (0);
}

static int
bus_lookup_address(const char *name)
{
	struct udev *udev;
	struct udev_enumerate *iter;
	struct udev_list_entry *entry, *list;
	struct udev_device *dev = NULL;
	const char *address;
	int ret;

	udev = udev_new();
	iter = udev_enumerate_new(udev);

	udev_enumerate_add_match_subsystem(iter, "librpc");
	udev_enumerate_add_match_property(iter, "name", name);
	udev_enumerate_scan_devices(iter);

	list = udev_enumerate_get_list_entry(iter);

	udev_list_entry_foreach(entry, list) {
		const char *ename;
		const char *path = udev_list_entry_get_name(entry);

		dev = udev_device_new_from_syspath(udev, path);
		ename = udev_device_get_property_value(dev, "name");

		if (g_strcmp0(name, ename) == 0)
			break;
	}

	if (dev == NULL)
		return (-1);

	address = udev_device_get_property_value(dev, "address");
	return (strtol(address, NULL, 0));
}

static int
bus_send_msg(void *arg, void *buf, size_t len, const int *fds __unused,
    size_t nfds __unused)
{
	struct bus_connection *conn = arg;
	struct librpc_message msg;

	msg.opcode = LIBRPC_REQUEST;
	msg.address = conn->bc_address;
	msg.status = 0;

	return (bus_netlink_send(&conn->bc_bn, &msg, buf, len));
}

static int
bus_netlink_open(struct bus_netlink *bn)
{
	int group = CN_LIBRPC_IDX;

	bn->bn_seq = 0;
	bn->bn_sock = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
	if (bn->bn_sock < 0)
		return (-1);

	setsockopt(bn->bn_sock, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP, &group,
	    sizeof(group));

	return (0);
}

static int
bus_netlink_send(struct bus_netlink *bn, struct librpc_message *msg,
    void *payload, size_t len)
{
	char buf[BUS_NL_MSGSIZE];
	struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
	struct cn_msg *cn = NLMSG_DATA(nlh);
	size_t size = NLMSG_SPACE(sizeof(msg) + len);

	nlh->nlmsg_seq = bn->bn_seq++;
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_type = NLMSG_DONE;
	nlh->nlmsg_flags = 0;

	cn->id.idx = CN_LIBRPC_IDX;
	cn->id.val = CN_LIBRPC_VAL;
	cn->seq = nlh->nlmsg_seq;
	cn->flags = 0;
	memcpy(cn->data, msg, sizeof(struct librpc_message));

	if (payload != NULL)
		memcpy(cn->data + sizeof(struct librpc_message), payload, len);

	if (send(bn->bn_sock, buf, size, 0) != 0)
		return (-1);

	return (0);
}

static int
bus_netlink_recv(struct bus_netlink *bn, struct librpc_message *msgo,
    void **payload, size_t *len)
{
	char buf[BUS_NL_MSGSIZE];
	struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
	struct cn_msg *cn = NLMSG_DATA(nlh);
	struct librpc_message *msg = (struct librpc_message *)(cn + 1);

	ssize_t msglen;

	msglen = recv(bn->bn_sock, buf, sizeof(buf), 0);
	nlh = (struct nlmsghdr *)buf;

	switch (nlh->nlmsg_type) {
		case NLMSG_ERROR:
			break;

		case NLMSG_DONE:
			*payload = g_malloc0(cn->len);
			memcpy(msgo, msg, sizeof(struct librpc_message));
			memcpy(*payload, msg + 1, cn->len);
			break;
	}
}

static void
bus_netlink_process_msg(struct bus_connection *conn, struct librpc_message *msg,
    void *payload, size_t len)
{

	switch (msg->opcode) {
	case LIBRPC_ACK:
		conn->bc_parent->rco_recv_msg(conn->bc_parent, payload, len,
		    NULL, 0, NULL);
		break;

	case LIBRPC_ARRIVE:
	case LIBRPC_DEPART:
		/* XXX implement */
		break;

	default:
		break;
	}
}

static void *
bus_reader(void *arg)
{
	struct librpc_message msg;
	struct bus_connection *conn = arg;
	void *frame;
	size_t len;

	for (;;) {
		if (bus_netlink_recv(&conn->bc_bn, &msg, &frame, &len) != 0)
			break;

		bus_netlink_process_msg(conn, &msg, frame, len);
	}

	conn->bc_parent->rco_close(conn->bc_parent);
	return (NULL);
}

DECLARE_TRANSPORT(bus_transport);
