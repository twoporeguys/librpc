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
#include <errno.h>
#include <glib.h>
#include <libsoup/soup.h>
#include "../../src/linker_set.h"
#include "../../src/internal.h"

struct loopback_channel
{
    	int				lc_number;
    	GHashTable *			lc_connections;
    	struct rpc_server *		lc_srv;
};

static int loopback_accept(struct loopback_channel *, struct rpc_connection *);
static int loopback_connect(struct rpc_connection *, const char *, rpc_object_t);
static int loopback_listen(struct rpc_server *, const char *, rpc_object_t);
static int loopback_abort(void *);
static int loopback_teardown(struct rpc_server *);
static int loopback_send_msg(void *, void *, size_t, const int *, size_t);
static void loopback_release(void *);

static GHashTable *loopback_channels = NULL;

static int
loopback_accept(struct loopback_channel *chan, struct rpc_connection *conn)
{
	struct rpc_connection *newconn;

	newconn = rpc_connection_alloc(chan->lc_srv);
	newconn->rco_send_msg = loopback_send_msg;
	newconn->rco_abort = loopback_abort;
	newconn->rco_arg = conn;
	newconn->rco_release = loopback_release;

	conn->rco_send_msg = loopback_send_msg;
	conn->rco_abort = loopback_abort;
	conn->rco_arg = newconn;
	conn->rco_release = loopback_release;

	g_hash_table_insert(chan->lc_connections, conn, newconn);
	chan->lc_srv->rs_accept(chan->lc_srv, newconn);
	return (0);
}

static int
loopback_connect(struct rpc_connection *conn, const char *uri_string,
    rpc_object_t extra __unused)
{
	SoupURI *uri;
	struct loopback_channel *chan;
	int number;

	uri = soup_uri_new(uri_string);
	if (uri == NULL) {
		rpc_set_last_error(EINVAL, "Invalid URI", NULL);
		return (-1);
	}

	number = (int)strtoul(uri->host, NULL, 10);
	chan = g_hash_table_lookup(loopback_channels, GINT_TO_POINTER(number));

	if (chan == NULL) {
		rpc_set_last_error(ENOENT, "Channel not found", NULL);
		soup_uri_free(uri);
		return (-1);
	}

	return (loopback_accept(chan, conn));
}

static int
loopback_listen(struct rpc_server *srv, const char *uri_string,
    rpc_object_t extra __unused)
{
	SoupURI *uri;
	struct loopback_channel *chan;

	uri = soup_uri_new(uri_string);
	if (uri == NULL)
		return (-1);

	chan = g_malloc0(sizeof(*chan));
	chan->lc_srv = srv;
	chan->lc_connections = g_hash_table_new(NULL, NULL);
	chan->lc_number = (int)strtoul(uri->host, NULL, 10);
	srv->rs_teardown = &loopback_teardown;
	srv->rs_arg = chan;

	if (loopback_channels == NULL)
		loopback_channels = g_hash_table_new(NULL, NULL);

	g_hash_table_insert(loopback_channels, GINT_TO_POINTER(chan->lc_number),
	    chan);
	return (0);
}

static int
loopback_send_msg(void *arg, void *buf, size_t len __unused, const int *fds,
    size_t nfds)
{
	struct rpc_connection *conn = arg;
	rpc_object_t obj = buf;

	return (conn->rco_recv_msg(conn, (const void *)obj, 0, (int *)fds,
	    nfds, NULL));
}

static int
loopback_abort(void *arg)
{
	struct rpc_connection *conn = arg;

	rpc_connection_close(conn);
	return (0);
}

static int
loopback_teardown(struct rpc_server *srv __unused)
{

	return (0);
}

static void
loopback_release(void *arg)
{

}

struct rpc_transport loopback_transport = {
	.name = "loopback",
	.schemas = {"loopback", NULL},
	.connect = loopback_connect,
	.listen = loopback_listen,
    	.flags = RPC_TRANSPORT_NO_SERIALIZE
};

DECLARE_TRANSPORT(loopback_transport);
