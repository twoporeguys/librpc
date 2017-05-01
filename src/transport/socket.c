/*+
 * Copyright 2015 Two Pore Guys, Inc.
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
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <gio/gio.h>
#include "../internal.h"

static struct rpc_transport_connection *socket_connect(const char *);
static struct rpc_transport_server *socket_listen(const char *);
static void *unix_event_loop(void *);

struct rpc_transport socket_transport = {
    .schemas = {"unix", "tcp", NULL},
    .connect = socket_connect,
    .listen = socket_listen
};

struct socket_server
{
    const char *	ss_uri;
    GSocketService	ss_service;
    conn_handler_t	ss_conn_handler;

};

struct socket_connection
{
    const char *	sc_uri;
    GSocketConnection *	sc_conn;
    message_handler_t	sc_message_handler;
    close_handler_t	sc_close_handler;
};

static struct rpc_transport_connection *
socket_connect(const char *uri)
{
	struct socket_connection *conn;
}

static struct rpc_transport_server *
socket_listen(const char *uri)
{
	struct socket_server *server;

}

void
unix_close(unix_conn_t *conn)
{
	shutdown(conn->unix_fd, SHUT_RDWR);
	pthread_join(conn->unix_thread, NULL);

	free(conn->unix_path);
	free(conn);
}

static int
unix_send_msg(void *arg, void *buf, size_t size, int *fds, size_t nfds)
{
	struct socket_connection *conn = arg;
	GSocketControlMessage *creds;
	GUnixFDList *fdlist;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct iovec iov;
	uint32_t header[2];
	int i;

	header[0] = 0xdeadbeef;
	header[1] = (uint32_t)size;

	memset(&msg, 0, sizeof(struct msghdr));
	iov.iov_base = header;
	iov.iov_len = sizeof(header);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_controllen = CMSG_SPACE(sizeof(struct cmsgcred)) +
	    CMSG_SPACE(nfds * sizeof(int));
	msg.msg_control = malloc(msg.msg_controllen);

	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_type = SCM_CREDS;
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_len = CMSG_LEN(sizeof(struct cmsgcred));

	if (nfds > 0) {
		cmsg = CMSG_NXTHDR(&msg, cmsg);
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_len = CMSG_LEN(nfds * sizeof(int));
		memcpy(CMSG_DATA(cmsg), fds, cmsg->cmsg_len);
	}

	if (xsendmsg(conn->unix_fd, &msg, 0) < 0)
		return (-1);

	if (xwrite(conn->unix_fd, buf, size) < 0)
		return (-1);

	return (0);
}

static int
socket_recv_msg(void *arg, void **frame, size_t *size,
    int **fds, size_t *nfds, struct rpc_credentials *creds)
{
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct cmsgcred *recv_creds;
	struct iovec iov;
	int *recv_fds = NULL;
	size_t recv_fds_count = 0;
	ssize_t recvd;
	uint32_t header[2];
	size_t length;

	if (xrecvmsg(conn->unix_fd, &msg, sizeof(uint32_t) * 2) < 0)
		return (-1);

	if (header[0] != 0xdeadbeef)
		return (-1);

	length = header[1];
	*frame = malloc(length);
	*size = length;

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	     cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_type == SCM_CREDS) {
			recv_creds = (struct cmsgcred *)CMSG_DATA(cmsg);
			continue;
		}

		if (cmsg->cmsg_type == SCM_RIGHTS) {
			recv_fds = (int *)CMSG_DATA(cmsg);
			recv_fds_count = CMSG_SPACE(cmsg);
		}
	}

	if (recv_creds != NULL) {
		creds->rcc_remote_pid = recv_creds->cmcred_pid;
		creds->rcc_remote_uid = recv_creds->cmcred_euid;
		creds->rcc_remote_gid = recv_creds->cmcred_gid;
		debugf("remote pid=%d, uid=%d, gid=%d", recv_creds->cmcred_pid,
		       recv_creds->cmcred_uid, recv_creds->cmcred_gid);

	}

	if (recv_fds != NULL) {
		int i;
		*fds = malloc(sizeof(int) * recv_fds_count);
		memcpy(*fds, recv_fds, sizeof(int) * recv_fds_count);
	}

	if (xread(conn->unix_fd, frame, size) < size) {
		free(frame);
		return (-1);
	}

	return (0);
}

void
unix_abort(unix_conn_t *conn)
{
	conn->unix_close_handler(conn, conn->unix_close_handler_arg);
}

int unix_get_fd(unix_conn_t *conn)
{

	return (conn->unix_fd);
}

static void
unix_process_msg(unix_conn_t *conn, void *frame, size_t size)
{
	conn->unix_message_handler(conn, frame, size,
	    conn->unix_message_handler_arg);
}

static void *
unix_event_loop(void *arg)
{
	unix_conn_t *conn = (unix_conn_t *)arg;
	struct kevent event;
	struct kevent change;
	int i, evs;
	int kq = kqueue();
	void *frame;
	size_t size;

	EV_SET(&change, conn->unix_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);

        if (kevent(kq, &change, 1, NULL, 0, NULL) < 0)
                goto out;

	for (;;) {
		evs = kevent(kq, NULL, 0, &event, 1, NULL);
		if (evs < 0) {
			if (errno == EINTR)
				continue;

			unix_abort(conn);
			goto out;
		}

		for (i = 0; i < evs; i++) {
			if (event.ident == conn->unix_fd) {
				if (event.flags & EV_EOF)
                                        goto out;

				if (event.flags & EV_ERROR)
                                        goto out;

				if (unix_recv_msg(conn, &frame, &size) < 0)
					continue;

				unix_process_msg(conn, frame, size);
			}
		}
	}

out:
        close(conn->unix_fd);
        close(kq);
        return (NULL);
}
