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

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <libsoup/soup.h>
#include <libudev.h>
#include <linux/netlink.h>
#include <linux/connector.h>
#include "../linker_set.h"
#include "../internal.h"
#include "../librpc.h"

#define	BUS_NODE	"/dev/librpc"
#define	BUS_MAX_MSG	65536

static int bus_connect(struct rpc_connection *, const char *, rpc_object_t);
static int bus_send_msg(void *, void *, size_t, const int *, size_t);
static int bus_abort(void *);
static int bus_get_fd(void *);

struct rpc_transport bus_transport = {
	.name = "bus",
	.schemas = {"bus", "usb", NULL},
	.connect = bus_connect,
	.listen = NULL
};

struct bus_connection
{
    	const char *		bc_name;
    	uint32_t		bc_address;
    	uint32_t 		bc_seq;
    	int			bc_sock;
    	struct rpc_connection *	bc_parent;
};

static int
bus_connect(struct rpc_connection *rco, const char *uri_string,
    rpc_object_t args __unused)
{
	SoupURI *uri;
	struct bus_connection *conn;
	struct udev *udev;
	struct udev_enumerate *iter;
	struct udev_list_entry *entry, *list;
	int ret;

	uri = soup_uri_new(uri_string);

	udev = udev_new();
	iter = udev_enumerate_new(udev);

	udev_enumerate_add_match_subsystem(iter, "librpc");
	udev_enumerate_add_match_property(iter, "name", uri->host);
	udev_enumerate_scan_devices(iter);

	list = udev_enumerate_get_list_entry(iter);

	udev_list_entry_foreach(entry, list) {

	}

	conn->bc_sock = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);

	conn = g_malloc0(sizeof(struct bus_connection));
	conn->bc_parent = rco;

	rco->rco_send_msg = &bus_send_msg;
	rco_>rco_ping = &bus_ping;
	rco->rco_abort = &bus_abort;
	rco->rco_arg = conn;
	return (0);
}

static int
bus_send_msg(void *arg, void *buf, size_t size, const int *fds, size_t nfds)
{
	struct bus_connection *conn = arg;
	struct librpc_message msg = { 0 };
	int ret;


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

	return (conn->bc_sock);
}

static int
bus_netlink_send(struct bus_connection *conn, struct librpc_msg *msg, size_t len)
{
	char buf[BUS_NL_MSGSIZE];
	struct nlmsghdr *nlh;
	struct cn_msg *cn = NLMSG_DATA(nlh);
	size_t size = NLMSG_SPACE(sizeof(msg) + msg->len));

	nlh->nlmsg_seq = conn->bc_seq++;
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_type = NLMSG_DONE;
	nlh->nlmsg_flags = 0;

	cn->id.idx = CN_LIBRPC_IDX;
	cn->id.val = CN_LIBRPC_VAL;
	cn->seq = nlh->nlmsg_seq;
	cn->flags = 0;

	memcpy(cn->data, msg, len);

	if (send(conn->bc_sock, buf, size, 0) != 0) {

	}

	return (0);
}

static int
bus_netlink_recv(struct bus_connection *conn, struct cn_msg **msg, size_t *len)
{
	struct nlmsghdr *nlh;
	char buf[BUS_NL_MSGSIZE];
	ssize_t msglen;

	msglen = recv(conn->bc_sock, buf, sizeof(buf), 0);
	nlh = (struct nlmsghdr *)buf;

	switch (nlh->nlmsg_type) {

	}
}

static void *
bus_reader(void *arg)
{
	struct bus_connection *conn = arg;
	void *frame;

	for (;;) {
		if (bus_netlink_recv(conn, &frame, &len, &fds, &nfds, &creds) != 0)
			break;

		if (conn->sc_parent->rco_recv_msg(conn->sc_parent, frame, len,
		    NULL, 0, NULL) != 0)
			break;

	}

	conn->sc_parent->rco_close(conn->sc_parent);
	return (NULL);
}


DECLARE_TRANSPORT(bus_transport);
