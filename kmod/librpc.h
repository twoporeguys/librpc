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

#ifndef _LIBRPC_H
#define _LIBRPC_H

#ifdef __KERNEL
#include <linux/uio.h>
#include <linux/connector.h>
#endif

#define CN_LIBRPC_IDX           (CN_NETLINK_USERS + 5)
#define CN_LIBRPC_VAL           1

struct librpc_endpoint
{
        char                    name[NAME_MAX];
        char                    serial[NAME_MAX];
    	char 			description[NAME_MAX];
};

enum librpc_opcode
{
        LIBRPC_QUERY = 0,
        LIBRPC_PING,
        LIBRPC_REQUEST,
        LIBRPC_RESPONSE,
        LIBRPC_ACK,
        LIBRPC_ARRIVE,
        LIBRPC_DEPART,
	LIBRPC_EVENT,
	LIBRPC_LOG
};

struct librpc_message
{
        uint8_t                 opcode;
        uint32_t                address;
        int                     status;
        char                    data[];
};

#ifdef __KERNEL__

struct librpc_device
{
        int                             address;
        const char *                    name;
        struct librpc_endpoint          endp;
        struct module *                 owner;
        struct device                   dev;
        const struct librpc_ops *       ops;
};

struct librpc_ops
{
        int (*open)(struct device *);
        void (*release)(struct device *);
        int (*enumerate)(struct device *, struct librpc_endpoint *);
        int (*ping)(struct device *);
        int (*request)(struct device *, void *, const void *, size_t);
};

#define to_librpc_device(dev)   container_of(dev, struct librpc_device, dev)

struct librpc_device *librpc_device_register(const char *name,
    struct device *dev, const struct librpc_ops *ops, struct module *owner);
void librpc_device_unregister(struct librpc_device *rpcdev);
void librpc_device_answer(struct device *, void *, const void *, size_t);
void librpc_device_error(struct device *, void *, int);
void librpc_device_event(struct device *, const void *, size_t);
void librpc_device_log(struct device *, const char *, size_t);

#endif /* __KERNEL__ */
#endif /* _LIBRPC_H */
