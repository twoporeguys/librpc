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
#include <string.h>
#include <libsoup/soup.h>
#include "../../src/linker_set.h"
#include "../../src/internal.h"

struct loopback_channel
{
    	int				lc_number;
    	struct rpc_server *		lc_srv;
};

struct looplock
{
	int				ll_refcnt;
	GMutex				ll_mtx;
};

struct loopback
{
	rpc_connection_t		lb_conn;
	struct loopback *		lb_peer;
	bool				lb_is_srv;
	bool				lb_closed;
        struct looplock *		lb_lock;
};


static int loopback_accept(struct loopback_channel *, struct rpc_connection *);
static int loopback_connect(struct rpc_connection *, const char *, rpc_object_t);
static int loopback_listen(struct rpc_server *, const char *, rpc_object_t);
static int loopback_abort(void *);
static int loopback_teardown(struct rpc_server *);
static int loopback_send_msg(void *, void *, size_t, const int *, size_t);
static void loopback_release(void *);
static int loopback_lock_free(struct loopback *);

static GHashTable *loopback_channels = NULL;

static int
loopback_accept(struct loopback_channel *chan, struct rpc_connection *conn)
{
	struct loopback *lb_s;
	struct loopback *lb_c;

	lb_c = g_malloc0(sizeof(*lb_c));
	lb_s = g_malloc0(sizeof(*lb_s));

	/*connections need a shared lock freed when both are closed*/
	lb_c->lb_lock = g_malloc0(sizeof(*lb_c->lb_lock));
	g_mutex_init(&lb_c->lb_lock->ll_mtx);
	lb_s->lb_lock = lb_c->lb_lock;
	lb_c->lb_lock->ll_refcnt = 2;

	lb_s->lb_conn = rpc_connection_alloc(chan->lc_srv);
	lb_s->lb_is_srv = true;
	lb_s->lb_conn->rco_send_msg = loopback_send_msg;
	lb_s->lb_conn->rco_abort = loopback_abort;
	lb_s->lb_conn->rco_arg = lb_s;
	lb_s->lb_conn->rco_release = loopback_release;

	lb_c->lb_conn = conn;
	conn->rco_send_msg = loopback_send_msg;
	conn->rco_abort = loopback_abort;
	conn->rco_arg = lb_c;
	conn->rco_release = loopback_release;

	lb_c->lb_peer = lb_s;
	lb_s->lb_peer = lb_c;

	if (chan->lc_srv->rs_accept(chan->lc_srv, lb_s->lb_conn) != 0) {
		debugf("loopback accept refused, s: %p, c: %p",
			lb_s->lb_conn, lb_c->lb_conn);
		lb_c->lb_conn->rco_closed = true;
		lb_c->lb_closed = true;

		lb_s->lb_conn->rco_close(lb_s->lb_conn);
		lb_c->lb_conn->rco_close(lb_c->lb_conn);

		return (-1);
	}
	debugf("loopback connection ACCEPTED, s: %p/%p, c: %p/%p",
			lb_s->lb_conn, lb_s, lb_c->lb_conn, lb_c);

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

	soup_uri_free(uri);

	if (chan == NULL) {
		rpc_set_last_error(ENOENT, "Channel not found", NULL);
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
	int host;
	int fail = false;

	uri = soup_uri_new(uri_string);

        if ((uri == NULL) || (uri->host == NULL) || !strlen(uri->host))
		fail = true;
	else {
		host = (int)strtoul(uri->host, NULL, 10);
		if (host == 0 && uri->host[0] != '0') {
			fail = true;
			soup_uri_free(uri);
		}
	}
	if (fail) {
                srv->rs_error = rpc_error_create(ENXIO, "No Such Address", NULL);
                debugf("Invalid loopback uri %s", uri_string);
                return (-1);
        }

	chan = g_malloc0(sizeof(*chan));
	chan->lc_srv = srv;
	chan->lc_number = host;
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
	struct loopback *lb = arg;
	struct rpc_connection *peer_conn;
	rpc_object_t obj = buf;
	int ret;

	g_mutex_lock(&lb->lb_lock->ll_mtx);
	if (lb->lb_closed || lb->lb_peer == NULL || lb->lb_peer->lb_closed) {
		g_mutex_unlock(&lb->lb_lock->ll_mtx);
		return (-1);
	}
	peer_conn = lb->lb_peer->lb_conn;
	rpc_connection_reference_retain(peer_conn);
	g_mutex_unlock(&lb->lb_lock->ll_mtx);

	rpc_retain(obj);
	ret = (peer_conn->rco_recv_msg(peer_conn, (const void *)obj, 0,
	    (int *)fds, nfds, NULL));
	rpc_connection_reference_release(peer_conn);
	return (ret);
}

static int
loopback_abort(void *arg)
{
	struct loopback *lb = arg;
	struct loopback *lb_peer = NULL;
	struct rpc_connection *conn;
	struct rpc_connection *peer = NULL;;

	if (lb == NULL)
		return (0);

	g_mutex_lock(&lb->lb_lock->ll_mtx);
	g_assert(!lb->lb_closed);
	if (lb->lb_closed) {
		debugf("Abort called on %p, %p already closed",
			lb, lb->lb_conn);
		g_mutex_unlock(&lb->lb_lock->ll_mtx);
		return (0);
	}
	if (lb->lb_peer) {
		g_assert_nonnull(lb->lb_peer->lb_peer);
		if (!lb->lb_is_srv) {
			lb_peer = lb->lb_peer;
			peer = lb_peer->lb_conn;
			rpc_connection_reference_retain(peer);
		}
		lb->lb_peer->lb_peer = NULL;
		lb->lb_peer = NULL;
	}

	debugf("Aborting  conn/arg %p/%p", conn, lb);

	conn = lb->lb_conn;
	lb->lb_closed = true;
	rpc_connection_reference_retain(conn);

	g_mutex_unlock(&lb->lb_lock->ll_mtx);

	conn->rco_close(conn);
	rpc_connection_reference_release(conn);

	if (peer != NULL) {
		loopback_abort(lb_peer);
		rpc_connection_reference_release(peer);
	}
	return (0);
}

static int
loopback_teardown(struct rpc_server *srv __unused)
{

	return (0);
}

static int
loopback_lock_free(struct loopback *lb)
{
	
	g_assert_nonnull(lb);

	g_mutex_lock(&lb->lb_lock->ll_mtx);

	g_assert(lb->lb_closed);
	g_assert_nonnull(lb->lb_lock);

	if (lb->lb_lock->ll_refcnt == 1) {
		g_mutex_unlock(&lb->lb_lock->ll_mtx);
		g_free(lb->lb_lock);
		lb->lb_lock = NULL;
		return 0;
	}
	g_assert(lb->lb_lock->ll_refcnt == 2);
	lb->lb_lock->ll_refcnt = 1;
	g_mutex_unlock(&lb->lb_lock->ll_mtx);
	return(1);
}

static void
loopback_release(void *arg)
{
	struct loopback *lb = arg;

	if (lb == NULL)
		return;
	g_assert(lb->lb_closed);

	loopback_lock_free(lb);
	g_free(lb);
}

struct rpc_transport loopback_transport = {
	.name = "loopback",
	.schemas = {"loopback", NULL},
	.connect = loopback_connect,
	.listen = loopback_listen,
    	.flags = RPC_TRANSPORT_NO_SERIALIZE
};

DECLARE_TRANSPORT(loopback_transport);
