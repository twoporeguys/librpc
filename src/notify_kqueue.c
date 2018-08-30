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

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <glib.h>
#include <sys/event.h>
#include "internal.h"
#include "notify.h"

static int kqueue_fd = -1;
static int id = 0;

void
notify_init(struct notify *notify)
{
	struct kevent kev;

	if (kqueue_fd == -1) {
		kqueue_fd = kqueue();
		g_assert(kqueue_fd != -1);
	}

	notify->fd = ++id;

	EV_SET(&kev, notify->fd, EVFILT_USER, EV_ADD, 0, 0, NULL);
	if (kevent(kqueue_fd, &kev, 1, NULL, 0, NULL) != 0)
		g_assert_not_reached();
}

void
notify_free(struct notify *notify)
{
	struct kevent kev;
	int ret;

	EV_SET(&kev, notify->fd, EVFILT_USER, EV_CLEAR | EV_DELETE, 0, 0, NULL);
	ret = kevent(kqueue_fd, &kev, 1, NULL, 0, NULL);

	/*
	 * I have no idea why kevent() returns EINPROGRESS, as it's not
	 * documented anywhere, so let's ignore it. ¯\_(ツ)_/¯
	 */
	if (ret != 0 && errno != EINPROGRESS)
		rpc_abort("kevent() failure: ret=%d, errno=%d", ret, errno);
}

int
notify_wait(struct notify *notify)
{

	return (notify_timedwait(notify, NULL));
}

int
notify_timedwait(struct notify *notify, const struct timespec *ts)
{
	struct kevent kev;
	int ret;

	for (;;) {
		ret = kevent(kqueue_fd, NULL, 0, &kev, 1, ts);
		if (ret < 0)
			return (-1);

		if (ret == 0)
			return (0);

		if (kev.ident == (uintptr_t)notify->fd)
			break;
	}

	EV_SET(&kev, notify->fd, EVFILT_USER, EV_CLEAR, 0, 0, NULL);
	kevent(kqueue_fd, &kev, 1, NULL, 0, NULL);
	return (1);
}

int
notify_signal(struct notify *notify)
{
	struct kevent kev;

	EV_SET(&kev, notify->fd, EVFILT_USER, 0, NOTE_TRIGGER, 1, NULL);
	return (kevent(kqueue_fd, &kev, 1, NULL, 0, NULL));
}
