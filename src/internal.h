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
#include <rpc/query.h>
#include <rpc/connection.h>
#include <rpc/service.h>
#include <rpc/server.h>
#include <rpc/bus.h>
#include <rpc/typing.h>
#include <stdio.h>
#include <setjmp.h>
#include <glib.h>
#ifdef LIBDISPATCH_SUPPORT
#include <dispatch/dispatch.h>
#endif
#include "linker_set.h"

#ifndef __unused
#define __unused __attribute__((unused))
#endif

#define	DECLARE_TRANSPORT(_transport)	DATA_SET(tp_set, _transport)
#define	DECLARE_SERIALIZER(_serializer)	DATA_SET(sr_set, _serializer)
#define	DECLARE_VALIDATOR(_validator)	DATA_SET(vr_set, _validator)
#define	DECLARE_TYPE_CLASS(_class)	DATA_SET(cs_set, _class)

#define	RPC_TRANSPORT_NO_SERIALIZE		(1 << 0)
#define	RPC_TRANSPORT_CREDENTIALS		(1 << 1)
#define RPC_TRANSPORT_FD_PASSING		(1 << 2)

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

#define	TYPE_REGEX	"(struct|union|type|enum) ([\\w\\.]+)(<(.*)>)?"
#define	INTERFACE_REGEX	"interface (\\w+)"
#define	INSTANCE_REGEX	"([\\w\\.]+)(<(.*)>)?"
#define	METHOD_REGEX	"method (\\w+)"
#define	PROPERTY_REGEX	"property (\\w+)"
#define	EVENT_REGEX	"event (\\w+)"

#ifdef _WIN32
typedef int uid_t;
typedef int gid_t;
#endif

struct rpc_connection;
struct rpc_credentials;
struct rpc_server;
struct rpct_validator;
struct rpct_error_context;

typedef enum rpc_dispatch_types
{
        RPC_TYPE_CALL = 1,
        RPC_TYPE_CONNECTION
} rpc_dispatch_type_t;

typedef int (*rpc_recv_msg_fn_t)(struct rpc_connection *, const void *, size_t,
    int *, size_t, struct rpc_credentials *);
typedef int (*rpc_send_msg_fn_t)(void *, void *, size_t, const int *, size_t);
typedef int (*rpc_abort_fn_t)(void *);
typedef int (*rpc_get_fd_fn_t)(void *);
typedef void (*rpc_release_fn_t)(void *);
typedef int (*rpc_close_fn_t)(struct rpc_connection *);
typedef int (*rpc_accept_fn_t)(struct rpc_server *, struct rpc_connection *);
typedef int (*rpc_teardown_fn_t)(struct rpc_server *);

typedef struct rpct_member *(*rpct_member_fn_t)(const char *, rpc_object_t,
    struct rpct_type *);
typedef bool (*rpct_validate_fn_t)(struct rpct_typei *, rpc_object_t,
    struct rpct_error_context *);
typedef rpc_object_t (*rpct_serialize_fn_t)(rpc_object_t);

struct rpc_query_iter
{
	rpc_object_t 		rqi_source;
	size_t 			rqi_idx;
	rpc_object_t 		rqi_rules;
	rpc_query_params_t 	rqi_params;
	bool			rqi_done;
	bool			rqi_initialized;
	uint32_t		rqi_limit;
};

struct rpc_binary_value
{
	uintptr_t 		rbv_ptr;
	size_t 			rbv_length;
	rpc_binary_destructor_t rbv_destructor;
};

struct rpc_shmem_block
{
    	int			rsb_fd;
    	off_t 			rsb_offset;
    	size_t 			rsb_size;
};

struct rpc_error_value
{
	int			rev_code;
	GString *		rev_message;
	rpc_object_t		rev_extra;
	rpc_object_t 		rev_stack;
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
	size_t			ro_line;
	size_t			ro_column;
	union rpc_value		ro_value;
	struct rpct_typei *	ro_typei;
};

struct rpc_call
{
	rpc_connection_t    	rc_conn;
	const char *        	rc_type;
	const char *		rc_path;
	const char *		rc_interface;
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
	const char *		rsu_name;
	const char *		rsu_path;
	const char *		rsu_interface;
    	int 			rsu_refcount;
    	GPtrArray *		rsu_handlers;
};

struct rpc_subscription_handler
{
	struct rpc_subscription *rsh_parent;
	rpc_handler_t 		rsh_handler;
};

struct rpc_inbound_call
{
	rpc_context_t 		ric_context;
    	rpc_connection_t    	ric_conn;
	rpc_instance_t 		ric_instance;
	rpc_object_t        	ric_id;
	rpc_object_t        	ric_args;
	rpc_abort_handler_t	ric_abort_handler;
	const char *        	ric_name;
	const char *		ric_interface;
	const char *		ric_path;
	struct rpc_if_method *	ric_method;
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
	bool			rco_has_creds;
	const char *        	rco_uri;
	rpc_error_handler_t 	rco_error_handler;
	rpc_handler_t		rco_event_handler;
	guint                 	rco_rpc_timeout;
	GHashTable *		rco_calls;
	GHashTable *		rco_inbound_calls;
    	GPtrArray *		rco_subscriptions;
    	GMutex			rco_subscription_mtx;
    	GMutex			rco_send_mtx;
	GMutex			rco_call_mtx;
    	GMainContext *		rco_mainloop;
    	GThreadPool *		rco_callback_pool;
	rpc_object_t 		rco_params;
    	int			rco_flags;
	bool			rco_closed;
#if LIBDISPATCH_SUPPORT
	dispatch_queue_t	rco_dispatch_queue;
#endif
        struct rpc_connection_statistics  rco_stats;
};

struct rpc_server
{
    	GMainContext *		rs_g_context;
    	GMainLoop *		rs_g_loop;
    	GThread *		rs_thread;
    	GList *			rs_connections;
    	GMutex			rs_mtx;
    	GCond			rs_cv;
	struct rpc_context *	rs_context;
    	const char *		rs_uri;
    	int 			rs_flags;
    	bool			rs_operational;
	bool			rs_paused;

    	/* Callbacks */
    	rpc_accept_fn_t		rs_accept;
    	rpc_teardown_fn_t	rs_teardown;
        rpc_server_event_handler_t rs_event;
    	void *			rs_arg;
};

union rpc_dispatch
{
        rpc_connection_t rd_conn;
        rpc_inbound_call rd_icall;
};

struct rpc_dispatch_item
{
        rpc_dispatch_type_t    rd_type;
        union rpc_dispatch     rd_item;
        int                    rd_code;
        rpc_object_t           rd_args;
};

struct rpc_client
{
    	GMainContext *		rci_g_context;
    	GMainLoop *		rci_g_loop;
    	GThread *		rci_thread;
    	rpc_connection_t 	rci_connection;
    	const char *		rci_uri;
	rpc_object_t 		rci_params;
};

struct rpc_instance
{
	char *			ri_path;
	char *			ri_descr;
	void *			ri_arg;
	rpc_context_t 		ri_context;
	GHashTable *		ri_interfaces;
	GHashTable *		ri_children;
	GHashTable *		ri_subscriptions;
	GHashTable *		ri_properties;
	GMutex			ri_mtx;
	GRWLock			ri_rwlock;
};

struct rpc_interface_priv
{
	char *			rip_name;
	char *			rip_description;
	void *			rip_arg;
	GHashTable *		rip_members;
	GMutex			rip_mtx;
};

struct rpc_property_cookie
{
	rpc_instance_t 		instance;
	rpc_object_t 		error;
	void *			arg;
	const char *		name;
};

struct rpc_context
{
    	GThreadPool *		rcx_threadpool;
	GHashTable *		rcx_instances;
	GPtrArray * 		rcx_servers;
	GRWLock			rcx_rwlock;
	rpc_instance_t 		rcx_root;

	/* Hooks */
	rpc_function_f		rcx_pre_call_hook;
	rpc_function_f		rcx_post_call_hook;
};

struct rpc_bus_transport
{
        void *(*open)(GMainContext *);
        void (*close)(void *);
        int (*ping)(void *, const char *);
        int (*enumerate)(void *, struct rpc_bus_node **, size_t *);
};

struct rpc_transport
{
	int (*connect)(struct rpc_connection *, const char *, rpc_object_t);
	int (*listen)(struct rpc_server *, const char *, rpc_object_t);
    	int flags;
        const struct rpc_bus_transport *bus_ops;
	const char *name;
	const char *schemas[];
};

struct rpc_serializer
{
    	int (*serialize)(rpc_object_t, void **, size_t *);
    	rpc_object_t (*deserialize)(const void *, size_t);
	const char *name;
};

struct rpct_context
{
	GHashTable *		files;
	GHashTable *		types;
	GHashTable *		interfaces;
	rpc_function_t		pre_call_hook;
	rpc_function_t 		post_call_hook;
};

struct rpct_file
{
	char *			path;
	const char *		description;
	const char *		ns;
	int64_t			version;
	GPtrArray *		uses;
	GHashTable *		types;
	GHashTable *		interfaces;
	rpc_object_t 		body;
};

/**
 * An RPC type.
 */
struct rpct_type
{
	rpct_class_t		clazz;
	char *			name;
	char *			description;
	struct rpct_file *	file;
	struct rpct_type *	parent;
	struct rpct_typei *	definition;
	bool			generic;
	GPtrArray *		generic_vars;
	GHashTable *		members;
	GHashTable *		constraints;
};

struct rpct_interface
{
	char *			name;
	char *			description;
	GHashTable *		members;
};

/**
 * This structure has two uses. It can hold either:
 * a) An instantiated (specialized or partially specialized) type
 * b) A "proxy" type that refers to parent's type generic variable
 */
struct rpct_typei
{
	bool			proxy;
	struct rpct_typei *	parent;
	struct rpct_type *	type;		/**< Only if proxy == false */
	const char *		variable;	/**< Only if proxy == true */
	char *			canonical_form;
	GHashTable *		specializations;
	GHashTable *		constraints;
	int 			refcount;
};

struct rpct_member
{
	char *			name;
	char *			description;
	struct rpct_typei *	type;
	struct rpct_type *	origin;
	GHashTable *		constraints;
};

struct rpct_if_member
{
	struct rpc_if_member	member;
	char *			description;
	GPtrArray *		arguments;
	struct rpct_typei *	result;
};

struct rpct_argument
{
	struct rpct_typei *	type;
	char *			name;
	char *			description;
};

struct rpct_error_context
{
	char *			path;
	GPtrArray *		errors;
};

struct rpct_class_handler
{
	rpct_class_t		id;
	const char *		name;
    	rpct_member_fn_t 	member_fn;
    	rpct_validate_fn_t 	validate_fn;
    	rpct_serialize_fn_t 	serialize_fn;
};

struct rpct_validation_error
{
	char *			path;
	char *			message;
	rpc_object_t 		obj;
	rpc_object_t 		extra;
};

struct rpct_validator
{
	const char *		type;
	const char * 		name;
	bool (*validate)(rpc_object_t, rpc_object_t, struct rpct_typei *, struct rpct_error_context *);
};

rpc_object_t rpc_prim_create(rpc_type_t type, union rpc_value val);

#if defined(__linux__)
rpc_object_t rpc_shmem_recreate(int fd, off_t offset, size_t size);
int rpc_shmem_get_fd(rpc_object_t shmem);
off_t rpc_shmem_get_offset(rpc_object_t shmem);
#endif

rpc_object_t rpc_error_create_from_gerror(GError *g_error);

void rpc_trace(const char *msg, rpc_object_t frame);
char *rpc_get_backtrace(void);
char *rpc_generate_v4_uuid(void);
gboolean rpc_kill_main_loop(void *arg);
int rpc_ptr_array_string_index(GPtrArray *arr, const char *str);

const struct rpc_transport *rpc_find_transport(const char *scheme);
const struct rpc_serializer *rpc_find_serializer(const char *name);
const struct rpct_validator *rpc_find_validator(const char *type,
    const char *name);
const struct rpct_class_handler *rpc_find_class_handler(const char *name,
    rpct_class_t cls);

void rpc_set_last_error(int code, const char *msg, rpc_object_t extra);
void rpc_set_last_rpc_error(rpc_object_t rpc_error);
void rpc_set_last_gerror(GError *error);
void rpc_set_last_errorf(int code, const char *fmt, ...);
rpc_connection_t rpc_connection_alloc(rpc_server_t server);
void rpc_connection_dispatch(rpc_connection_t, rpc_object_t);
int rpc_context_dispatch(rpc_context_t, struct rpc_dispatch_item *);
int rpc_server_dispatch(rpc_server_t, struct rpc_inbound_call *);
void rpc_server_connection_change(rpc_server_t server, struct rpc_dispatch_item *itm);
void rpc_connection_send_err(rpc_connection_t, rpc_object_t, int,
    const char *descr, ...);
void rpc_connection_send_errx(rpc_connection_t, rpc_object_t, rpc_object_t);
void rpc_connection_send_response(rpc_connection_t, rpc_object_t, rpc_object_t);
void rpc_connection_send_fragment(rpc_connection_t, rpc_object_t, int64_t,
    rpc_object_t);
void rpc_connection_send_end(rpc_connection_t, rpc_object_t, int64_t);
void rpc_connection_close_inbound_call(struct rpc_inbound_call *);
void rpc_connection_release_call(struct rpc_inbound_call *);

void rpc_bus_event(rpc_bus_event_t, struct rpc_bus_node *);

void rpct_typei_free(struct rpct_typei *inst);
void rpct_add_error(struct rpct_error_context *ctx, const char *fmt, ...);
void rpct_derive_error_context(struct rpct_error_context *newctx,
    struct rpct_error_context *oldctx, const char *name);
void rpct_release_error_context(struct rpct_error_context *ctx);
bool rpct_validate_instance(struct rpct_typei *typei, rpc_object_t obj,
    struct rpct_error_context *errctx);
bool rpct_run_validators(struct rpct_typei *typei, rpc_object_t obj,
    struct rpct_error_context *errctx);
struct rpct_typei *rpct_instantiate_type(const char *decl,
    struct rpct_typei *parent, struct rpct_type *ptype,
    struct rpct_file *origin);

#endif /* LIBRPC_INTERNAL_H */
