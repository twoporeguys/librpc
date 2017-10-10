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
 */

#include <Block.h>
#include <errno.h>
#include <glib.h>
#include <rpc/bus.h>
#include "internal.h"

static rpc_bus_event_handler_t rpc_bus_event_handler = NULL;
static GMainContext *rpc_g_main_context = NULL;
static GMainLoop *rpc_g_main_loop = NULL;
static GThread *rpc_g_main_thread = NULL;
static GMutex rpc_bus_mtx = { 0 };
static void *rpc_bus_context = NULL;
static gint rpc_bus_refcnt = 0;

static void *
rpc_bus_worker(void *arg __unused)
{

	rpc_g_main_loop = g_main_loop_new(rpc_g_main_context, false);
	g_main_loop_run(rpc_g_main_loop);
	return (NULL);
}

int
rpc_bus_open(void)
{
	const struct rpc_transport *bus;

	g_mutex_lock(&rpc_bus_mtx);

	if (rpc_bus_refcnt > 0) {
		g_assert_nonnull(rpc_bus_context);
		goto done;
	}

	bus = rpc_find_transport("bus");
	if (bus == NULL) {
		errno = ENXIO;
		rpc_bus_context = NULL;
		goto done;
	}

	if (bus->bus_ops == NULL) {
		errno = ENXIO;
		rpc_bus_context = NULL;
		goto done;
	}

	rpc_g_main_context = g_main_context_new();
	rpc_g_main_thread = g_thread_new("bus", rpc_bus_worker, NULL);
	rpc_bus_context = bus->bus_ops->open(rpc_g_main_context);
	rpc_bus_refcnt++;

done:
	g_mutex_unlock(&rpc_bus_mtx);
	return (rpc_bus_context != NULL ? 0 : -1);
}

int
rpc_bus_close(void)
{
	const struct rpc_transport *bus;
	int ret = 0;

	g_mutex_lock(&rpc_bus_mtx);

	if (rpc_bus_refcnt > 0) {
		g_assert_nonnull(rpc_bus_context);
		rpc_bus_refcnt--;
		goto done;
	}

	bus = rpc_find_transport("bus");
	if (bus == NULL) {
		errno = ENXIO;
		ret = -1;
		goto done;
	}

	if (bus->bus_ops == NULL) {
		errno = ENXIO;
		ret = -1;
		goto done;
	}

	if (rpc_bus_context == NULL)
		goto done;

	bus->bus_ops->close(rpc_bus_context);
	g_main_loop_quit(rpc_g_main_loop);
	g_main_loop_unref(rpc_g_main_loop);
	g_main_context_unref(rpc_g_main_context);
	g_thread_join(rpc_g_main_thread);
	rpc_bus_context = NULL;
	rpc_bus_refcnt--;

done:
	g_mutex_unlock(&rpc_bus_mtx);
	return (ret);
}

int
rpc_bus_ping(const char *serial)
{
	const struct rpc_transport *bus;
	int ret = 0;

	g_mutex_lock(&rpc_bus_mtx);
	bus = rpc_find_transport("bus");
	if (bus == NULL) {
		errno = ENXIO;
		ret = -1;
		goto done;
	}

	if (bus->bus_ops == NULL) {
		errno = ENXIO;
		ret = -1;
		goto done;
	}

	ret = bus->bus_ops->ping(rpc_bus_context, serial);

done:
	g_mutex_unlock(&rpc_bus_mtx);
	return (ret);
}

int
rpc_bus_enumerate(struct rpc_bus_node **resultp)
{
	const struct rpc_transport *bus;
	struct rpc_bus_node *result = NULL;
	size_t count;
	int ret = 0;

	g_mutex_lock(&rpc_bus_mtx);
	bus = rpc_find_transport("bus");
	if (bus == NULL) {
		errno = ENXIO;
		ret = -1;
		goto done;
	}

	if (bus->bus_ops == NULL) {
		errno = ENXIO;
		ret = -1;
		goto done;
	}

	if (bus->bus_ops->enumerate(rpc_bus_context, &result, &count) != 0) {
		ret = -1;
		goto done;
	}

	*resultp = result;
	ret = (int)count;

done:
	g_mutex_unlock(&rpc_bus_mtx);
	return (ret);
}

void
rpc_bus_free_result(struct rpc_bus_node *result)
{

	g_free(result);

}

void
rpc_bus_register_event_handler(rpc_bus_event_handler_t handler)
{

	rpc_bus_event_handler = Block_copy(handler);
}

void
rpc_bus_unregister_event_handler(void)
{

	if (rpc_bus_event_handler != NULL) {
		Block_release(rpc_bus_event_handler);
		rpc_bus_event_handler = NULL;
	}
}

void
rpc_bus_event(rpc_bus_event_t event, struct rpc_bus_node *node)
{

	if (rpc_bus_event_handler != NULL)
		rpc_bus_event_handler(event, node);
}
