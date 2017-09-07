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
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include "librpc.h"
#include "librpc_user.h"

struct librpc_user_endpoint
{
	struct librpc_endpoint		info;
	uint32_t 			portid;
};

static struct librpc_ops librpc_user_ops = {
	.open = librpc_user_open,
	.release = librpc_user_release,
	.enumerate = librpc_user_enumerate,
	.ping = librpc_user_ping,
	.request = librpc_user_request
};

static struct cb_id librpc_user_cb_id = {
	.idx = CN_LIBRPC_IDX,
	.val = CN_LIBRPC_USER_VAL
};

static int
librpc_user_open(struct device *dev)
{

}

static void
librpc_user_release(struct device *dev)
{

}

static int
librpc_user_enumerate(struct device *dev, struct librpc_endpoint *endp)
{

}

static int
librpc_user_ping(struct device *dev)
{

}

static int
librpc_user_request(struct device *dev, void *cookie, const void *buf,
    size_t len)
{

}

static void
librpc_user_new_endpoint(struct librpc_message *msg)
{
	struct librpc_user_endpoint *endp;

	endp = kzalloc(sizeof(*endp), GFP_KERNEL);
	if (endp == NULL)
		return;


}

static void
librpc_user_cn_callback(struct cn_msg *cn, struct netlink_skb_parms *nsp)
{
	struct librpc_message *msg = (struct librpc_messsage *)(cn + 1);

	switch (msg->opcode) {
	case LIBRPC_USER_REGISTER:
		librpc_user_new_endpoint(msg);
		break;

	case LIBRPC_USER_UNREGISTER:
	}
}

static int __init
librpc_user_init(void)
{
	int ret;

	ret = cn_add_callback(&librpc_user_cb_id, "librpc-user",
	    librpc_user_cn_callback);
}

static void __exit
librpc_user_exit(void)
{

}

MODULE_AUTHOR("Jakub Klama");
MODULE_LICENSE("Dual BSD/GPL");

module_init(librpc_user_init);
module_exit(librpc_user_exit);

