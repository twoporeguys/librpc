/*
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
 */

#include <unistd.h>
#include <errno.h>
#include <glib.h>
#include <libsoup/soup.h>
#include "../linker_set.h"
#include "../internal.h"

static int fd_connect(struct rpc_connection *, const char *, rpc_object_t);
static int fd_listen(struct rpc_server *, const char *, rpc_object_t);

struct fd_connection
{
	GThread *		thread;
	struct rpc_connection *	parent;
	int			fd;
};

struct fd_server
{
	struct rpc_server *	server;
	struct fd_connection	conn;
};

static const struct rpc_transport fd_transport = {
	.name = "fd",
	.schemas = {"fd", NULL},
	.connect = fd_connect,
	.listen = fd_listen,
};

ssize_t
xread(int fd, void *buf, size_t nbytes)
{
	ssize_t ret, done = 0;

	while (done < (ssize_t)nbytes) {
		ret = read(fd, (buf + done), nbytes - done);
		if (ret == 0)
			return (-1);

		if (ret < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;

			return (-1);
		}

		done += ret;
	}

	return (done);
}

ssize_t
xwrite(int fd, void *buf, size_t nbytes)
{
	ssize_t ret, done = 0;

	while (done < (ssize_t)nbytes) {
		ret = write(fd, (buf + done), nbytes - done);
		if (ret == 0)
			return (-1);

		if (ret < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;

			return (-1);
		}

		done += ret;
	}

	return (done);
}

static int
fd_recv_msg(struct fd_connection *fdconn, void **frame, size_t *size)
{
	uint32_t header[4];
	size_t length;

	if (xread(fdconn->fd, header, sizeof(header)) < 0)
		return (-1);

	if (header[0] != 0xdeadbeef)
		return (-1);

	length = header[1];
	*frame = g_malloc(length);
	*size = length;

	if (xread(fdconn->fd, *frame, length) < 0) {
		g_free(*frame);
		return (-1);
	}

	return (0);
}

static int
fd_send_msg(void *arg, void *buf, size_t size, const int *fds __unused,
    size_t nfds __unused)
{
	struct fd_connection *fdconn = arg;
	uint32_t header[4] = { 0xdeadbeef, (uint32_t)size, 0, 0 };

	if (xwrite(fdconn->fd, header, sizeof(header)) < 0)
		return (-1);

	if (xwrite(fdconn->fd, buf, size) < 0)
		return (-1);

	return (0);
}

static int
fd_abort(void *arg)
{
	struct fd_connection *fdconn = arg;

	return (close(fdconn->fd));
}

static int
fd_get_fd(void *arg)
{
	struct fd_connection *fdconn = arg;

	return (fdconn->fd);
}

static void
fd_release(void *arg)
{

}

static void *
fd_reader(void *arg)
{
	struct fd_connection *fdconn = arg;
	void *frame;
	size_t len;

	for (;;) {
		if (fd_recv_msg(fdconn, &frame, &len) != 0)
			break;

		if (fdconn->parent->rco_recv_msg(fdconn->parent, frame, len,
		    NULL, 0, NULL) != 0) {
			g_free(frame);
			break;
		}

		g_free(frame);
	}

	fdconn->parent->rco_close(fdconn->parent);
	return (NULL);
}

static int
fd_teardown(struct rpc_server *srv)
{
	struct fd_server *fdsrv = srv->rs_arg;

	return (fd_abort(&fdsrv->conn));
}

static int
fd_connect(struct rpc_connection *rco, const char *uri_string,
    rpc_object_t params)
{
	SoupURI *uri;
	struct fd_connection *fdconn;
	int fd;

	if (params != NULL)
		fd = rpc_fd_get_value(params);
	else {
		uri = soup_uri_new(uri_string);
		if (uri == NULL)
			return (-1);

		fd = (int)strtoul(uri->host, NULL, 10);
		soup_uri_free(uri);
	}

	fdconn = g_malloc0(sizeof(*fdconn));
	fdconn->parent = rco;
	fdconn->fd = fd;
	rco->rco_send_msg = fd_send_msg;
	rco->rco_abort = fd_abort;
	rco->rco_get_fd = fd_get_fd;
	rco->rco_release = fd_release;
	rco->rco_arg = fdconn;
	fdconn->thread = g_thread_new("fd client", fd_reader, fdconn);

	return (0);
}

int
fd_listen(struct rpc_server *srv, const char *uri_string,
    rpc_object_t params)
{
	SoupURI *uri;
	struct fd_server *fdsrv;
	int fd;


	if (params != NULL)
		fd = rpc_fd_get_value(params);
	else {
		uri = soup_uri_new(uri_string);
		if (uri == NULL)
			return (-1);

		fd = (int)strtoul(uri->host, NULL, 10);
		soup_uri_free(uri);
	}

	fdsrv = g_malloc0(sizeof(*fdsrv));
	fdsrv->server = srv;
	srv->rs_teardown = fd_teardown;
	srv->rs_arg = fdsrv;
	fdsrv->conn.fd = fd;
	fdsrv->conn.parent = rpc_connection_alloc(srv);
	fdsrv->conn.parent->rco_send_msg = fd_send_msg;
	fdsrv->conn.parent->rco_abort = fd_abort;
	fdsrv->conn.parent->rco_get_fd = fd_get_fd;
	fdsrv->conn.parent->rco_release = fd_release;
	fdsrv->conn.parent->rco_arg = &fdsrv->conn;
	fdsrv->conn.thread = g_thread_new("fd client", fd_reader, &fdsrv->conn);

	srv->rs_accept(srv, fdsrv->conn.parent);
	return (0);
}

DECLARE_TRANSPORT(fd_transport);
