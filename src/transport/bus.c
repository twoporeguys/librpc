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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
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

#define	BUS_NL_MSGSIZE	16384

struct bus_netlink;
struct bus_connection;
typedef void (*bus_netlink_cb_t)(void *, struct librpc_message *, void *,
    size_t);

static void *bus_open(GMainContext *);
static void bus_close(void *);
static int bus_connect(struct rpc_connection *, const char *, rpc_object_t);
static int bus_send_msg(void *, const void *, size_t, const int *, size_t);
static int bus_ping(void *, const char *);
static int bus_enumerate(void *, struct rpc_bus_node **, size_t *);
static int bus_abort(void *);
static int bus_get_fd(void *);
static int bus_netlink_open(struct bus_netlink *);
static int bus_netlink_close(struct bus_netlink *);
static int bus_netlink_send(struct bus_netlink *, struct librpc_message *,
    const void *, size_t);
static int bus_netlink_recv(struct bus_netlink *);
static int bus_lookup_address(const char *, uint32_t *);
static void bus_process_message(void *, struct librpc_message *, void *, size_t);
static void *bus_reader(void *);
static void bus_release(void *);

static const struct rpc_bus_transport bus_transport_ops = {
	.open = bus_open,
	.close = bus_close,
	.ping = bus_ping,
	.enumerate = bus_enumerate,
};

static const struct rpc_transport bus_transport = {
	.name = "bus",
	.schemas = {"bus", "usb", NULL},
	.bus_ops = &bus_transport_ops,
	.connect = bus_connect,
	.listen = NULL
};

struct bus_ack
{
    	GMutex			ba_mtx;
    	GCond			ba_cv;
    	bool			ba_done;
    	int			ba_status;
};

struct bus_netlink
{
    	int 			bn_sock;
    	uint32_t 		bn_seq;
    	GHashTable *		bn_ack;
    	GMutex			bn_mtx;
    	GThread *		bn_thread;
    	bus_netlink_cb_t	bn_callback;
    	void *			bn_arg;
};

struct bus_connection
{
    	const char *		bc_name;
    	uint32_t		bc_address;
    	struct bus_netlink	bc_bn;
    	struct rpc_connection *	bc_parent;
};

static void *
bus_open(GMainContext *context __unused)
{
	struct bus_netlink *bn;

	bn = g_malloc0(sizeof(*bn));
	if (bus_netlink_open(bn) != 0) {
		free(bn);
		return (NULL);
	}

	return (bn);
}

static void
bus_close(void *arg)
{
	struct bus_netlink *bn = arg;

	(void)bus_netlink_close(bn);
}

static int
bus_connect(struct rpc_connection *rco, const char *uri_string,
    rpc_object_t args __unused)
{
	SoupURI *uri;
	struct bus_connection *conn;

	uri = soup_uri_new(uri_string);
	conn = g_malloc0(sizeof(struct bus_connection));
	conn->bc_parent = rco;

	if (bus_lookup_address(uri->host, &conn->bc_address) != 0) {
		rpc_set_last_error(ENOENT, "Cannot find device", NULL);
		g_free(conn);
		return (-1);
	}

	if (bus_netlink_open(&conn->bc_bn) != 0) {
		rpc_set_last_error(EINVAL, "Cannot open device", NULL);
		g_free(conn);
		return (-1);
	}

	conn->bc_bn.bn_callback = &bus_process_message;
	conn->bc_bn.bn_arg = conn;
	rco->rco_send_msg = &bus_send_msg;
	rco->rco_abort = &bus_abort;
	rco->rco_get_fd = &bus_get_fd;
	rco->rco_arg = conn;
	rco->rco_release = bus_release;
	return (0);
}


static int
bus_abort(void *arg)
{
	struct bus_connection *conn = arg;

	bus_netlink_close(&conn->bc_bn);
	g_free(conn);
	return (0);
}

static int
bus_get_fd(void *arg)
{
	struct bus_connection *conn = arg;

	return (conn->bc_bn.bn_sock);
}

static int
bus_ping(void *arg, const char *name)
{
	struct bus_netlink *bn = arg;
	struct librpc_message msg;
	uint32_t address;

	if (bus_lookup_address(name, &address))
		return (-1);

	msg.opcode = LIBRPC_PING;
	msg.address = address;
	msg.status = 0;

	return (bus_netlink_send(bn, &msg, NULL, 0));
}

static int
bus_enumerate(void *arg __unused, struct rpc_bus_node **resultp, size_t *countp)
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
		if (dev == NULL)
			continue;

		if (g_strcmp0(udev_device_get_sysname(dev), "librpc") == 0)
			continue;

		result = g_realloc(result, (count + 1) * sizeof(*result));
		result[count].rbn_address = (uint32_t)strtol(
		    udev_device_get_sysattr_value(dev, "address"), NULL, 10);
		result[count].rbn_name = g_strdup(
		    udev_device_get_sysattr_value(dev, "name"));
		result[count].rbn_description = g_strdup(
		    udev_device_get_sysattr_value(dev, "description"));
		result[count].rbn_serial = g_strdup(
		    udev_device_get_sysattr_value(dev, "serial"));
		count++;
	}

	*resultp = result;
	*countp = count;
	return (0);
}

static int
bus_lookup_address(const char *serial, uint32_t *address)
{
	struct udev *udev;
	struct udev_enumerate *iter;
	struct udev_list_entry *entry, *list;
	struct udev_device *dev = NULL;
	const char *str;

	udev = udev_new();
	iter = udev_enumerate_new(udev);

	udev_enumerate_add_match_subsystem(iter, "librpc");
	udev_enumerate_add_match_sysattr(iter, "serial", serial);
	udev_enumerate_scan_devices(iter);

	list = udev_enumerate_get_list_entry(iter);

	udev_list_entry_foreach(entry, list) {
		const char *eserial;
		const char *path = udev_list_entry_get_name(entry);

		dev = udev_device_new_from_syspath(udev, path);
		eserial = udev_device_get_property_value(dev, "serial");

		if (g_strcmp0(serial, eserial) == 0)
			break;
	}

	if (dev == NULL)
		return (-1);

	str = udev_device_get_sysattr_value(dev, "address");
	*address = (uint32_t)strtol(str, NULL, 10);
	return (0);
}

static int
bus_send_msg(void *arg, const void *buf, size_t len, const int *fds __unused,
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
	struct sockaddr_nl sa;
	int group = CN_LIBRPC_IDX;

	bn->bn_seq = 0;
	bn->bn_sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_CONNECTOR);
	if (bn->bn_sock < 0) {
		rpc_set_last_error(errno, strerror(errno), NULL);
		return (-1);
	}

	g_mutex_init(&bn->bn_mtx);
	bn->bn_ack = g_hash_table_new(NULL, NULL);

	sa.nl_family = AF_NETLINK;
	sa.nl_groups = (uint32_t)-1;
	sa.nl_pid = 0;
	if (bind(bn->bn_sock, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
		rpc_set_last_error(errno, strerror(errno), NULL);
		return (-1);
	}

	setsockopt(bn->bn_sock, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP, &group,
	    sizeof(group));

	bn->bn_thread = g_thread_new("bus reader", &bus_reader, bn);
	return (0);
}

static int
bus_netlink_close(struct bus_netlink *bn)
{
	close(bn->bn_sock);
	return (0);
}

static int
bus_netlink_send(struct bus_netlink *bn, struct librpc_message *msg,
    const void *payload, size_t len)
{
	char buf[BUS_NL_MSGSIZE];
	struct bus_ack ack;
	struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
	struct cn_msg *cn = NLMSG_DATA(nlh);
	size_t size = NLMSG_SPACE(sizeof(*cn) + sizeof(*msg) + len);

	nlh->nlmsg_seq = bn->bn_seq++;
	nlh->nlmsg_pid = (uint32_t)getpid();
	nlh->nlmsg_len = (uint32_t)size;
	nlh->nlmsg_type = NLMSG_DONE;
	nlh->nlmsg_flags = NLM_F_REQUEST;

	cn->id.idx = CN_LIBRPC_IDX;
	cn->id.val = CN_LIBRPC_VAL;
	cn->seq = nlh->nlmsg_seq;
	cn->ack = 100;
	cn->flags = 0;
	cn->len = sizeof(struct librpc_message) + (uint16_t)len;
	memcpy(cn->data, msg, sizeof(struct librpc_message));

	if (payload != NULL)
		memcpy(cn->data + sizeof(struct librpc_message), payload, len);

	g_mutex_lock(&bn->bn_mtx);

	ack.ba_done = false;
	ack.ba_status = 0;
	g_mutex_init(&ack.ba_mtx);
	g_cond_init(&ack.ba_cv);
	g_hash_table_insert(bn->bn_ack, GUINT_TO_POINTER(cn->seq), &ack);

	if (send(bn->bn_sock, buf, size, 0) != (ssize_t)size)
		return (-1);

	g_mutex_unlock(&bn->bn_mtx);
	g_mutex_lock(&ack.ba_mtx);

	while (!ack.ba_done)
		g_cond_wait(&ack.ba_cv, &ack.ba_mtx);

	g_mutex_unlock(&ack.ba_mtx);
	return (ack.ba_status);
}

static int
bus_netlink_recv(struct bus_netlink *bn)
{
	char buf[BUS_NL_MSGSIZE];
	struct bus_ack *ack;
	struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
	struct cn_msg *cn = NLMSG_DATA(nlh);
	struct librpc_message *msg = (struct librpc_message *)(cn + 1);
	struct librpc_message *out;
	struct librpc_endpoint *endp;
	struct rpc_bus_node node;
	void *payload;
	ssize_t msglen = 0;

	msglen = recv(bn->bn_sock, buf, BUS_NL_MSGSIZE, 0);
	nlh = (struct nlmsghdr *)buf;

	debugf("message: type=%d, seq=%d, len=%d", nlh->nlmsg_type, cn->seq,
	    cn->len);

	switch (nlh->nlmsg_type) {
		case NLMSG_ERROR:
			out = NULL;
			payload = NULL;
			break;

		case NLMSG_DONE:
			out = g_malloc(sizeof(*out));
			payload = g_malloc(cn->len);
			memcpy(out, msg, sizeof(struct librpc_message));
			memcpy(payload, msg + 1, cn->len);
			break;

		default:
			return (0);
	}

	switch (msg->opcode) {
	case LIBRPC_ARRIVE:
	case LIBRPC_DEPART:
		endp = (struct librpc_endpoint *)(msg + 1);
		node.rbn_name = endp->name;
		node.rbn_description = endp->description;
		node.rbn_serial = endp->serial;
		node.rbn_address = msg->address;

		if (msg->opcode == LIBRPC_ARRIVE)
			rpc_bus_event(RPC_BUS_ATTACHED, &node);

		if (msg->opcode == LIBRPC_DEPART)
			rpc_bus_event(RPC_BUS_DETACHED, &node);

		break;

	case LIBRPC_ACK:
		g_mutex_lock(&bn->bn_mtx);
		ack = g_hash_table_lookup(bn->bn_ack, GUINT_TO_POINTER(cn->seq));
		g_mutex_unlock(&bn->bn_mtx);

		if (ack != NULL) {
			g_mutex_lock(&ack->ba_mtx);
			ack->ba_done = true;
			ack->ba_status = msg->status;
			g_cond_broadcast(&ack->ba_cv);
			g_mutex_unlock(&ack->ba_mtx);
		}
		break;

	default:
		if (bn->bn_callback != NULL) {
			bn->bn_callback(bn->bn_arg, msg, payload,
			    cn->len - sizeof(*cn));
		}
		break;
	}


	return (0);
}

static void
bus_release(void *arg)
{

}

static void
bus_process_message(void *arg, struct librpc_message *msg, void *payload,
    size_t len)
{
	struct bus_connection *conn = arg;

	switch (msg->opcode) {
	case LIBRPC_RESPONSE:
		conn->bc_parent->rco_recv_msg(conn->bc_parent, payload, len,
		    NULL, 0);
		break;
	}
}

static void *
bus_reader(void *arg)
{
	struct bus_netlink *bn = arg;

	for (;;) {
		if (bus_netlink_recv(bn) != 0)
			break;
	}

	//conn->bc_parent->rco_close(conn->bc_parent);
	return (NULL);
}

DECLARE_TRANSPORT(bus_transport);
