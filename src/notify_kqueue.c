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
#include "notify.h"

static int kqueue_fd = -1;
static int id = 0;

void
notify_init(struct notify *notify)
{
	struct kevent kev;

	if (kqueue_fd == -1)
		kqueue_fd = kqueue();

	notify->fd = ++id;
	EV_SET(&kev, notify->fd, EVFILT_USER, EV_ADD, NOTE_FFCOPY, 0, NULL);
	kevent(kqueue_fd, &kev, 1, NULL, 0, NULL);

void
notify_free(struct notify *notify)
{

	close(notify->fd);
	g_free(notify);
}

int
notify_wait(struct notify *notify)
{
	eventfd_t value;

	if (eventfd_read(notify->fd, &value) < 0) {
		printf("eventfd_read() = %d\n", errno);
		return (-1);
	}

	return ((int)value);
}

int
notify_timedwait(struct notify *notify, const struct timeval *ts)
{

}

int
notify_signal(struct notify *notify)
{

	return (eventfd_write(notify->fd, 1));
}
