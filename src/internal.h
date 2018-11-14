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

#include <stdatomic.h>
#include <stdio.h>
#include <glib.h>
#include <rpc/object.h>
#include <rpc/query.h>
#include <rpc/client.h>
#include <rpc/connection.h>
#include <rpc/service.h>
#include <rpc/server.h>
#include <rpc/bus.h>
#include <rpc/typing.h>
#ifdef LIBDISPATCH_SUPPORT
#include <dispatch/dispatch.h>
#endif
#include "linker_set.h"
#include "notify.h"

#ifndef __unused
#define __unused __attribute__((unused))
#endif

#define INTERNAL_LINKAGE		__attribute__((visibility("hidden")))
#define STRINGIFY(x)			#x
#define TOSTRING(x)			STRINGIFY(x)

#define	DECLARE_TRANSPORT(_transport)	DATA_SET(tp_set, _transport)
#define	DECLARE_SERIALIZER(_serializer)	DATA_SET(sr_set, _serializer)
#define	DECLARE_VALIDATOR(_validator)	DATA_SET(vr_set, _validator)
#define	DECLARE_TYPE_CLASS(_class)	DATA_SET(cs_set, _class)

#define	RPC_TRANSPORT_NO_SERIALIZE		(1 << 0)
#define	RPC_TRANSPORT_CREDENTIALS		(1 << 1)
#define RPC_TRANSPORT_FD_PASSING		(1 << 2)
#define	RPC_TRANSPORT_NO_RPCT_SERIALIZE		(1 << 3)

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

#define	TYPE_REGEX	"(struct|union|type|container|enum) ([\\w\\.]+)(<(.*)>)?"
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

typedef int (*rpc_recv_msg_fn_t)(struct rpc_connection *, const void *, size_t,
    int *, size_t, struct rpc_credentials *);
typedef int (*rpc_send_msg_fn_t)(void *, const void *, size_t, const int *, size_t);
typedef int (*rpc_abort_fn_t)(void *);
typedef int (*rpc_get_fd_fn_t)(void *);
typedef void (*rpc_release_fn_t)(void *);
typedef int (*rpc_close_fn_t)(struct rpc_connection *);
typedef int (*rpc_accept_fn_t)(struct rpc_server *, struct rpc_connection *);
typedef bool (*rpc_valid_fn_t)(struct rpc_server *);
typedef int (*rpc_teardown_fn_t)(struct rpc_server *);

typedef struct rpct_member *(*rpct_member_fn_t)(const char *, rpc_object_t,
    struct rpct_type *);
typedef bool (*rpct_validate_fn_t)(struct rpct_typei *, rpc_object_t,
    struct rpct_error_context *);
typedef rpc_object_t (*rpct_serialize_fn_t)(rpc_object_t);

typedef bool (*rpct_validator_fn_t)(rpc_object_t, rpc_object_t,
    struct rpct_typei *, struct rpct_error_context *);

typedef void (*rpc_fn_respond_fn_t)(void *, rpc_object_t);
typedef void (*rpc_fn_error_fn_t)(void *, int , const char *, va_list ap);
typedef void (*rpc_fn_error_ex_fn_t)(void *, rpc_object_t);
typedef int (*rpc_fn_start_strm_fn_t)(void *);
typedef int (*rpc_fn_yield_fn_t)(void *, rpc_object_t);
typedef void (*rpc_fn_end_fn_t)(void *);
typedef void (*rpc_fn_kill_fn_t)(void *);
typedef bool (*rpc_fn_should_abt_fn_t)(void *);
typedef void (*rpc_fn_set_abt_h_fn_t)(void *, rpc_abort_handler_t);

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

struct rpc_call
{
	rpc_connection_t    	rc_conn;
	rpc_context_t 		rc_context;
	rpc_call_type_t        	rc_type;
	char *			rc_path;
	char *			rc_interface;
	char *        		rc_method_name;
	rpc_object_t        	rc_id;
	rpc_object_t        	rc_args;
	rpc_object_t		rc_err;
	volatile int		rc_refcount;
	struct notify		rc_notify;
	GMutex			rc_mtx;
	GMutex			rc_ref_mtx;
	GSource *		rc_timeout;
	GQueue *		rc_queue;
	bool			rc_timedout;
	rpc_callback_t    	rc_callback;
	atomic_int_fast64_t	rc_producer_seqno;
	atomic_int_fast64_t	rc_consumer_seqno; /* also rc_seqno */
	uint64_t 		rc_prefetch;
	rpc_instance_t 		rc_instance;
	rpc_abort_handler_t	rc_abort_handler;
	struct rpc_if_method *	rc_if_method;
	void *			rc_m_arg;
	bool			rc_streaming;
	bool			rc_responded;
	bool			rc_ended;
	bool			rc_aborted;
};

struct rpc_credentials
{
    	uid_t			rcc_uid;
    	gid_t			rcc_gid;
    	pid_t 			rcc_pid;
};

struct rpc_fn_callbacks
{
	rpc_fn_respond_fn_t	rcf_fn_respond;
	rpc_fn_error_fn_t	rcf_fn_error;
	rpc_fn_error_ex_fn_t	rcf_fn_error_ex;
	rpc_fn_start_strm_fn_t	rcf_fn_start_stream;
	rpc_fn_yield_fn_t	rcf_fn_yield;
	rpc_fn_end_fn_t		rcf_fn_end;
	rpc_fn_kill_fn_t	rcf_fn_kill;
	rpc_fn_should_abt_fn_t	rcf_should_abort;
	rpc_fn_set_abt_h_fn_t	rcf_set_async_abort_handler;
};

struct rpc_connection
{
	struct rpc_server *	rco_server;
	struct rpc_client *	rco_client;
	struct rpc_context *	rco_rpc_context;
    	struct rpc_credentials	rco_creds;
	bool			rco_has_creds;
	bool			rco_supports_fd_passing;
	const char *        	rco_uri;
	char *			rco_endpoint_address;
	rpc_error_handler_t 	rco_error_handler;
	rpc_handler_t		rco_event_handler;
	rpc_raw_handler_t 	rco_raw_handler;
	guint                 	rco_rpc_timeout;
	GHashTable *		rco_calls;
	GHashTable *		rco_inbound_calls;
    	GPtrArray *		rco_subscriptions;
    	GMutex			rco_subscription_mtx;
	GMutex			rco_mtx;
	GMutex			rco_ref_mtx;
	GMutex			rco_send_mtx;
	GRWLock			rco_icall_rwlock;
	GRWLock			rco_call_rwlock;
	GMainContext *		rco_main_context;
	rpc_object_t            rco_error;
    	GThreadPool *		rco_callback_pool;
	rpc_object_t 		rco_params;
    	int			rco_flags;
	bool			rco_closed;
	bool			rco_aborted;
	bool			rco_released;
	volatile int		rco_refcnt;
#if LIBDISPATCH_SUPPORT
	dispatch_queue_t	rco_dispatch_queue;
#endif

    	/* Callbacks */
	rpc_recv_msg_fn_t	rco_recv_msg;
	rpc_send_msg_fn_t	rco_send_msg;
	rpc_abort_fn_t 		rco_abort;
	rpc_close_fn_t		rco_close;
    	rpc_get_fd_fn_t 	rco_get_fd;
	rpc_release_fn_t	rco_release;
	void *			rco_arg;
	struct rpc_fn_callbacks rco_fn_cbs;
};

struct rpc_server
{
    	GMainContext *		rs_g_context;
    	GMainLoop *		rs_g_loop;
    	GThread *		rs_thread;
    	GList *			rs_connections;
	GQueue *		rs_calls;
    	GMutex			rs_mtx;
	GMutex			rs_calls_mtx;
    	GCond			rs_cv;
	GRWLock			rs_connections_rwlock;
	struct rpc_context *	rs_context;
    	const char *		rs_uri;
    	int 			rs_flags;
    	bool			rs_operational;
	bool			rs_paused;
	bool			rs_closed;
	bool			rs_threaded_teardown;
        rpc_object_t            rs_error;
	volatile int		rs_refcnt;
	int			rs_conn_made;
	int			rs_conn_refused;
	volatile int		rs_conn_closed;
	int			rs_conn_aborted;
	rpc_object_t 		rs_params;
	rpc_server_ev_handler_t rs_event_handler;

    	/* Callbacks */
	rpc_valid_fn_t		rs_valid;
    	rpc_accept_fn_t		rs_accept;
    	rpc_teardown_fn_t	rs_teardown;
	rpc_teardown_fn_t	rs_teardown_end;
    	void *			rs_arg;
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
	bool			ri_destroyed;
	int	 		ri_refcnt;
	rpc_context_t 		ri_context;
	GHashTable *		ri_interfaces;
	GMutex			ri_mtx;
	GCond			ri_cv;
	GRWLock			ri_rwlock;
};

struct rpc_interface_priv
{
	const char *		rip_name;
	char *			rip_description;
	void *			rip_arg;
	GHashTable *		rip_members;
	GRWLock			rip_rwlock;
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
	GRWLock			rcx_server_rwlock;
	rpc_instance_t 		rcx_root;
	GAsyncQueue *		rcx_emit_queue;
	GThread *		rcx_emit_thread;

	/* Hooks */
	rpc_function_t		rcx_pre_call_hook;
	rpc_function_t		rcx_post_call_hook;
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
	bool (*is_fd_passing)(struct rpc_connection *);
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
	GHashTable *		typei_cache;
	rpc_function_t		pre_call_hook;
	rpc_function_t 		post_call_hook;
};

struct rpct_file
{
	char *			path;
	const char *		description;
	const char *		ns;
	bool			loaded;
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
	char *			origin;
	struct rpct_file *	file;
	struct rpct_type *	parent;
	struct rpct_typei *	definition;
	struct rpct_typei *	value_type;
	bool			generic;
	GPtrArray *		generic_vars;
	GHashTable *		members;
	GHashTable *		constraints;
};

struct rpct_interface
{
	char *			name;
	char *			description;
	char *			origin;
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
	volatile int		refcnt;
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
	bool			opt;
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
	rpct_serialize_fn_t 	deserialize_fn;
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
	rpct_validator_fn_t 	validate;
};

INTERNAL_LINKAGE rpc_object_t rpc_prim_create(rpc_type_t type,
    union rpc_value val);

#if defined(__linux__)
INTERNAL_LINKAGE rpc_object_t rpc_shmem_recreate(int fd, off_t offset,
    size_t size);
INTERNAL_LINKAGE int rpc_shmem_get_fd(rpc_object_t shmem);
INTERNAL_LINKAGE off_t rpc_shmem_get_offset(rpc_object_t shmem);
#endif

INTERNAL_LINKAGE rpc_object_t rpc_error_create_from_gerror(GError *g_error);

INTERNAL_LINKAGE void rpc_abort(const char *fmt, ...);
INTERNAL_LINKAGE void rpc_trace(const char *msg, const char *ident,
    rpc_object_t frame);
INTERNAL_LINKAGE char *rpc_get_backtrace(void);
INTERNAL_LINKAGE char *rpc_generate_v4_uuid(void);
INTERNAL_LINKAGE gboolean rpc_kill_main_loop(void *arg);
INTERNAL_LINKAGE int rpc_ptr_array_string_index(GPtrArray *arr,
    const char *str);

INTERNAL_LINKAGE const struct rpc_transport *rpc_find_transport(
    const char *scheme);
INTERNAL_LINKAGE const struct rpc_serializer *rpc_find_serializer(
    const char *name);
INTERNAL_LINKAGE const struct rpct_validator *rpc_find_validator(
    const char *type,
    const char *name);
INTERNAL_LINKAGE const struct rpct_class_handler *rpc_find_class_handler(
    const char *name, rpct_class_t cls);

INTERNAL_LINKAGE void rpc_set_last_error(int code, const char *msg,
    rpc_object_t extra);
INTERNAL_LINKAGE void rpc_set_last_rpc_error(rpc_object_t rpc_error);
INTERNAL_LINKAGE void rpc_set_last_gerror(GError *error);
INTERNAL_LINKAGE void rpc_set_last_errorf(int code, const char *fmt, ...)
    __attribute__((__format__(__printf__, 2, 3)));
INTERNAL_LINKAGE rpc_connection_t rpc_connection_alloc(rpc_server_t server);
INTERNAL_LINKAGE void rpc_connection_dispatch(rpc_connection_t, rpc_object_t);
INTERNAL_LINKAGE int rpc_connection_retain(rpc_connection_t);
INTERNAL_LINKAGE int rpc_connection_release(rpc_connection_t);
INTERNAL_LINKAGE int rpc_context_dispatch(rpc_context_t, struct rpc_call *);
INTERNAL_LINKAGE int rpc_server_dispatch(rpc_server_t, struct rpc_call *);
INTERNAL_LINKAGE void rpc_server_release(rpc_server_t);
INTERNAL_LINKAGE void rpc_server_quit(rpc_server_t);
INTERNAL_LINKAGE void rpc_server_disconnect(rpc_server_t, rpc_connection_t);
INTERNAL_LINKAGE GMainContext *rpc_server_get_main_context(rpc_server_t);
INTERNAL_LINKAGE GMainContext *rpc_client_get_main_context(rpc_client_t);

INTERNAL_LINKAGE void rpc_connection_send_err(rpc_connection_t, rpc_object_t,
    int, const char *descr, ...);
INTERNAL_LINKAGE void rpc_connection_send_errx(rpc_connection_t, rpc_object_t,
    rpc_object_t);
INTERNAL_LINKAGE void rpc_connection_send_response(rpc_connection_t,
    rpc_object_t, rpc_object_t);
INTERNAL_LINKAGE void rpc_connection_send_start_stream(rpc_connection_t,
    rpc_object_t, int64_t);
INTERNAL_LINKAGE void rpc_connection_send_fragment(rpc_connection_t,
    rpc_object_t, int64_t, rpc_object_t);
INTERNAL_LINKAGE void rpc_connection_send_end(rpc_connection_t, rpc_object_t,
    int64_t);
INTERNAL_LINKAGE void rpc_connection_close_inbound_call(struct rpc_call *);
INTERNAL_LINKAGE int rpc_connection_call_retain(struct rpc_call *call);
INTERNAL_LINKAGE int rpc_connection_call_release(struct rpc_call *call);

INTERNAL_LINKAGE rpc_instance_t rpc_instance_retain(rpc_instance_t);
INTERNAL_LINKAGE void rpc_instance_release(rpc_instance_t);

INTERNAL_LINKAGE void rpc_bus_event(rpc_bus_event_t, struct rpc_bus_node *);

INTERNAL_LINKAGE void rpct_add_error(struct rpct_error_context *ctx,
    rpc_object_t extra,  const char *fmt, ...);
INTERNAL_LINKAGE void rpct_derive_error_context(
    struct rpct_error_context *newctx, struct rpct_error_context *oldctx,
    const char *name);
INTERNAL_LINKAGE void rpct_release_error_context(
    struct rpct_error_context *ctx);
INTERNAL_LINKAGE bool rpct_validate_instance(struct rpct_typei *typei,
    rpc_object_t obj, struct rpct_error_context *errctx);
INTERNAL_LINKAGE bool rpct_run_validators(struct rpct_typei *typei,
    rpc_object_t obj, struct rpct_error_context *errctx);
INTERNAL_LINKAGE struct rpct_typei *rpct_instantiate_type(const char *decl,
    struct rpct_typei *parent, struct rpct_type *ptype,
    struct rpct_file *origin);

INTERNAL_LINKAGE void rpc_function_respond_impl(void *cookie,
    rpc_object_t object);
INTERNAL_LINKAGE void rpc_function_error_impl(void *cookie, int code,
    const char *message, va_list ap);
INTERNAL_LINKAGE void rpc_function_error_ex_impl(void *cookie,
    rpc_object_t exception);
INTERNAL_LINKAGE int rpc_function_start_stream_impl(void *cookie);
INTERNAL_LINKAGE int rpc_function_yield_impl(void *cookie,
    rpc_object_t fragment);
INTERNAL_LINKAGE void rpc_function_end_impl(void *cookie);
INTERNAL_LINKAGE void rpc_function_kill_impl(void *cookie);
INTERNAL_LINKAGE bool rpc_function_should_abort_impl(void *cookie);
INTERNAL_LINKAGE void rpc_function_set_async_abort_handler_impl(void *cookie,
    rpc_abort_handler_t handler);

#endif /* LIBRPC_INTERNAL_H */
