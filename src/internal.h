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
#include <rpc/service.h>
#include <rpc/connection.h>
#include <stdio.h>
#include <glib.h>

union rpc_value
{
    GHashTable *	rv_dict;
    GArray *		rv_list;
    GString *		rv_str;
    GDateTime *		rv_datetime;
    uint64_t 		rv_ui;
    int64_t		rv_i;
    bool		rv_b;
    double		rv_d;
    uintptr_t 		rv_ptr;
    int 		rv_fd;
};

struct rpc_object
{
    rpc_type_t		ro_type;
    volatile int	ro_refcnt;
    union rpc_value	ro_value;
    size_t 		ro_size;
};

struct rpc_call
{
    rpc_connection_t *  rc_conn;
    const char *        rc_type;
    const char *        rc_method;
    rpc_object_t        rc_id;
    rpc_object_t        rc_args;
    rpc_call_status_t   rc_status;
    rpc_object_t        rc_result;
    pthread_cond_t      rc_cv;
    pthread_mutex_t     rc_mtx;
    rpc_callback_t *    rc_callback;
    void *              rc_callback_arg;
    int                 rc_seqno;
};

struct rpc_connection
{
    const char *        rco_uri;
    rpc_callback_t *	rco_error_handler;
    void *              rco_error_handler_arg;
    rpc_callback_t *	rco_event_handler;
    void *              rco_event_handler_arg;
    int                 rco_rpc_timeout;
    GHashTable *	rco_calls;
};

struct rpc_credentials
{
    uid_t		rcc_remote_uid;
    gid_t		rcc_remote_gid;
    pid_t 		rcc_remote_pid;
};

struct rpc_server
{

};

struct rpc_context
{
    GHashTable *	rcx_methods;
};

struct rpc_transport_connection
{
    	void *priv;
    	int (*recv_msg)(void **, size_t *, int **, size_t *,
	    struct rpc_credentials *);
    	int (*send_msg)(void *, size_t, int *, size_t );
    	int (*close)(void);
};

struct rpc_transport_server
{
    	void *priv;
	int (*accept)(struct rpc_transport_connection *);
};

struct rpc_transport
{
    struct rpc_transport_connection *(*connect)(const char *);
    struct rpc_transport_server *(*listen)(const char *);
    const char *schemas[];
};

typedef struct rpc_transport_connection rpc_transport_connection_t;

typedef void (^conn_handler_t)(rpc_transport_connection_t *);
typedef void (^message_handler_t)(void *frame, size_t len);
typedef void (^close_handler_t)(void);

int rpc_context_dispatch(rpc_context_t context, const char *method, rpc_object_t *);

ssize_t xread(int, void *, size_t);
ssize_t xwrite(int, void *, size_t);
ssize_t xrecvmsg(int, struct msghdr *, int);
ssize_t xsendmsg(int, struct msghdr *, int);
char *xfgetln(FILE *);

#endif //LIBRPC_INTERNAL_H
