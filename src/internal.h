/*
 * Copyright 2015-2017 Two Pore Guys, Inc.
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

#ifndef LIBRPC_INTERNAL_H
#define LIBRPC_INTERNAL_H

#include <rpc/object.h>
#include <rpc/connection.h>
#include <rpc/service.h>
#include <rpc/server.h>
#include <rpc/bus.h>
#include <stdio.h>
#include <setjmp.h>
#include <glib.h>
#include "linker_set.h"

#ifndef __unused
#define __unused __attribute__((unused))
#endif

#define	DECLARE_TRANSPORT(_transport)	DATA_SET(tp_set, _transport)
#define	DECLARE_SERIALIZER(_serializer)	DATA_SET(sr_set, _serializer)

#define	RPC_TRANSPORT_NO_SERIALIZE		(1 << 0)
#define	RPC_TRANSPORT_CREDENTIALS		(1 << 1)
#define RPC_TRANSPORT_FD_PASSING		(1 << 2)

#define RPC_DEBUG	0
#if RPC_DEBUG
#define debugf(...) 				\
    do { 					\
    	fprintf(stderr, "%s: ", __func__);	\
    	fprintf(stderr, __VA_ARGS__);		\
    	fprintf(stderr, "\n");			\
    } while(0);
#else
#define debugf(...)
#endif

struct rpc_connection;
struct rpc_credentials;
struct rpc_server;

typedef int (*rpc_recv_msg_fn_t)(struct rpc_connection *, const void *, size_t,
    int *, size_t, struct rpc_credentials *);
typedef int (*rpc_send_msg_fn_t)(void *, void *, size_t, const int *, size_t);
typedef int (*rpc_abort_fn_t)(void *);
typedef int (*rpc_get_fd_fn_t)(void *);
typedef int (*rpc_close_fn_t)(struct rpc_connection *);
typedef int (*rpc_accept_fn_t)(struct rpc_server *, struct rpc_connection *);
typedef int (*rpc_teardown_fn_t)(struct rpc_server *);

struct rpc_binary_value
{
	uintptr_t 		ptr;
	size_t 			length;
	bool 			copy;
};

struct rpc_shmem_block
{
    	int			rsb_fd;
    	off_t 			rsb_offset;
    	size_t 			rsb_size;
};

struct rpc_error_value
{
	int			code;
	GString *		message;
	rpc_object_t		extra;
    	rpc_object_t 		stack;
};

union rpc_value
{
	GHashTable *		rv_dict;
	GPtrArray *		rv_list;
	GString *		rv_str;
	GDateTime *		rv_datetime;
	uint64_t 		rv_ui;
	int64_t			rv_i;
	bool			rv_b;
	double			rv_d;
	int			rv_fd;
	struct rpc_binary_value rv_bin;
	struct rpc_error_value	rv_error;
#if defined(__linux__)
    	struct rpc_shmem_block  rv_shmem;
#endif
};

struct rpc_object
{
	rpc_type_t		ro_type;
	volatile int		ro_refcnt;
	union rpc_value		ro_value;
};

struct rpc_call
{
	rpc_connection_t    	rc_conn;
	const char *        	rc_type;
	const char *        	rc_method;
	rpc_object_t        	rc_id;
	rpc_object_t        	rc_args;
	rpc_call_status_t   	rc_status;
	rpc_object_t        	rc_result;
	GCond      		rc_cv;
	GMutex			rc_mtx;
    	GAsyncQueue *		rc_queue;
    	GSource *		rc_timeout;
	rpc_callback_t    	rc_callback;
	uint64_t               	rc_seqno;
};

struct rpc_subscription
{
    	int 			rsu_refcount;
    	GPtrArray *		rsu_handlers;
};

enum rpc_inbound_state
{
    	RPC_INBOUND_WAITING,
    	RPC_INBOUND_FRAGMENT_REQUESTED
};

struct rpc_inbound_call
{
    	rpc_connection_t    	ric_conn;
	rpc_object_t        	ric_id;
	rpc_object_t        	ric_args;
	const char *        	ric_name;
    	struct rpc_method *	ric_method;
    	GMutex			ric_mtx;
    	GCond			ric_cv;
    	volatile int64_t	ric_producer_seqno;
    	volatile int64_t	ric_consumer_seqno;
    	void *			ric_arg;
    	bool			ric_streaming;
    	bool			ric_responded;
    	bool			ric_ended;
    	bool			ric_aborted;
};

struct rpc_credentials
{
    	uid_t			rcc_uid;
    	gid_t			rcc_gid;
    	pid_t 			rcc_pid;
};

struct rpc_connection
{
	struct rpc_server *	rco_server;
    	struct rpc_client *	rco_client;
    	struct rpc_credentials	rco_creds;
	const char *        	rco_uri;
	rpc_error_handler_t 	rco_error_handler;
	rpc_handler_t		rco_event_handler;
	guint                 	rco_rpc_timeout;
	GHashTable *		rco_calls;
	GHashTable *		rco_inbound_calls;
    	GHashTable *		rco_event_listeners;
    	GHashTable *		rco_subscriptions;
    	GMutex			rco_subscription_mtx;
    	GMutex			rco_send_mtx;
    	GMainContext *		rco_mainloop;
    	GThreadPool *		rco_callback_pool;
    	int			rco_flags;
	bool			rco_closed;

    	/* Callbacks */
	rpc_recv_msg_fn_t	rco_recv_msg;
	rpc_send_msg_fn_t	rco_send_msg;
	rpc_abort_fn_t 		rco_abort;
	rpc_close_fn_t		rco_close;
    	rpc_get_fd_fn_t 	rco_get_fd;
	void *			rco_arg;
};

struct rpc_server
{
    	GMainContext *		rs_g_context;
    	GMainLoop *		rs_g_loop;
    	GThread *		rs_thread;
    	GList *			rs_connections;
    	GHashTable *		rs_subscriptions;
    	GMutex			rs_subscription_mtx;
    	GMutex			rs_mtx;
    	GCond			rs_cv;
	struct rpc_context *	rs_context;
    	const char *		rs_uri;
    	int 			rs_flags;
    	bool			rs_operational;

    	/* Callbacks */
    	rpc_accept_fn_t		rs_accept;
    	rpc_teardown_fn_t	rs_teardown;
    	void *			rs_arg;
};

struct rpc_client
{
    	GMainContext *		rci_g_context;
    	GMainLoop *		rci_g_loop;
    	GThread *		rci_thread;
    	rpc_connection_t 	rci_connection;
    	const char *		rci_uri;
    	int 			rci_flags;
};

struct rpc_context
{
    	GHashTable *		rcx_methods;
    	GThreadPool *		rcx_threadpool;
};

struct rpc_transport
{
	int (*connect)(struct rpc_connection *, const char *, rpc_object_t);
	int (*listen)(struct rpc_server *, const char *, rpc_object_t);
    	int (*ping)(const char *);
    	int (*enumerate)(struct rpc_bus_node **, size_t *);
    	int flags;
	const char *name;
	const char *schemas[];
};

struct rpc_serializer
{
    	int (*serialize)(rpc_object_t, void **, size_t *);
    	rpc_object_t (*deserialize)(const void *, size_t);
	const char *name;
};

rpc_object_t rpc_prim_create(rpc_type_t type, union rpc_value val);

#if defined(__linux__)
rpc_object_t rpc_shmem_recreate(int fd, off_t offset, size_t size);
int rpc_shmem_get_fd(rpc_object_t shmem);
off_t rpc_shmem_get_offset(rpc_object_t shmem);
#endif

void rpc_trace(const char *msg, rpc_object_t frame);
char *rpc_get_backtrace(void);

const struct rpc_transport *rpc_find_transport(const char *scheme);
const struct rpc_serializer *rpc_find_serializer(const char *name);

void rpc_set_last_error(GError *error);
rpc_connection_t rpc_connection_alloc(rpc_server_t server);
void rpc_connection_dispatch(rpc_connection_t, rpc_object_t);
int rpc_context_dispatch(rpc_context_t, struct rpc_inbound_call *);
int rpc_server_dispatch(rpc_server_t, struct rpc_inbound_call *);
void rpc_connection_send_err(rpc_connection_t, rpc_object_t, int,
    const char *descr, ...);
void rpc_connection_send_errx(rpc_connection_t, rpc_object_t, rpc_object_t);
void rpc_connection_send_response(rpc_connection_t, rpc_object_t, rpc_object_t);
void rpc_connection_send_fragment(rpc_connection_t, rpc_object_t, int64_t,
    rpc_object_t);
void rpc_connection_send_end(rpc_connection_t, rpc_object_t, int64_t);
void rpc_connection_close_inbound_call(struct rpc_inbound_call *);

#endif //LIBRPC_INTERNAL_H