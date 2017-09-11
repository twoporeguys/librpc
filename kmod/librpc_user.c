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

static int librpc_user_send(struct cn_msg *, uint32_t);
static void librpc_user_new_endpoint(struct librpc_message *, uint32_t);
static void librpc_user_cn_callback(struct cn_msg *,
    struct netlink_skb_parms *);

struct librpc_user_endpoint
{
	struct device *			dev;
	struct librpc_endpoint		info;
	uint32_t 			portid;
};

struct librpc_user_completion
{
	struct list_head		list;
	uint32_t			seq;
	uint32_t			portid;
	struct completion		done;
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

static LIST_HEAD(librpc_user_completions);
static struct class *librpc_user_class;

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
	struct librpc_user_endpoint *uendp = dev_get_drvdata(dev);

	*endp = endp->endp;
	return (0);
}

static int
librpc_user_ping(struct device *dev)
{

	return (0);
}

static int
librpc_user_request(struct device *dev, void *cookie, const void *buf,
    size_t len)
{
	struct librpc_user_endpoint *uendp = dev_get_drvdata(dev);
	struct {
		struct cn_msg cn;
		struct librpc_message msg;
		struct librpc_user_call ucall;
		char data[len];
	} packet;

	packet.cn.id = librpc_user_cn_id;
	packet.cn.seq = resp_seq++;
	packet.cn.ack = 0;
	packet.cn.len = sizeof(packet);
	packet.cn.flags = 0;
	packet.msg.opcode = LIBRPC_USER_REQUEST;
	packet.msg.status = 0;
	packet.ucall.cookie = (uintptr_t)cookie;
	memcpy(&packet.ucall.data, buf, len);	

	librpc_user_send(&packet.cn, uendp->portid, 0, GFP_KERNEL);
	return (0);
}

static int
librpc_user_send(struct cn_msg *cn, uint32_t portid)
{
	struct librpc_user_completion cmpl;
	int ret;

	cmpl.portid = portid;
	cmpl.seqno = cn.seq;
	init_completion(&cmpl.done);
	list_add(&cmpl.list, &librpc_user_completions);	

	ret = cn_send_msg(cn, portid, 0, GFP_KERNEL);
	if (ret != 0)
		return (ret);

	wait_for_completion_interruptible(&cmpl);
	list_del(&cmpl.list);
	return (0);	
}

static void
librpc_user_new_endpoint(struct librpc_message *msg, uint32_t portid)
{
	struct librpc_endpoint *endp;
	struct librpc_user_endpoint *uendp;

	endp = (struct librpc_endpoint *)&msg->data;
	uendp = kzalloc(sizeof(*uendp), GFP_KERNEL);
	if (uendp == NULL)
		return;

	uendp->info = *endp;
	uendp->portid = portid;
	uendp->dev = device_create(librpc_user_class, NULL, MKDEV(0, 0),
	    uendp, "librpc_user%u", portid);
	librpc_device_register(endp->name, uendp->dev, &librpc_user_ops,
	    THIS_MODULE);
}

static void
librpc_user_cn_callback(struct cn_msg *cn, struct netlink_skb_parms *nsp)
{
	struct librpc_message *msg = (struct librpc_messsage *)(cn + 1);
	struct librpc_user_endpoint *uendp;
	size_t msglen = cn->len - sizeof(*msg);

	switch (msg->opcode) {
	case LIBRPC_USER_REGISTER:
		librpc_user_new_endpoint(msg, nsp->portid);
		break;

	case LIBRPC_USER_UNREGISTER:
		librpc_user_remove_endpoint(msg, nsp->portid);
		break;

	case LIBRPC_USER_RESPONSE:
		uendp = librpc_user_find_endpoint(nsp->portid);
		if (uendp == NULL)
			break;

		librpc_device_answer(uendp->dev, msg->data, msglen);
		break;				

	case LIBRPC_USER_ERROR:
		uendp = librpc_user_find_endpoint(nsp->portid);
		if (uendp == NULL)
			break;

		librpc_device_error(

	case LIBRPC_USER_LOG:
		uendp = librpc_user_find_endpoint(nsp->portid);
		if (uendp == NULL)
			break;

		librpc_device_log(uendp->dev, msg->data, msglen);
		break;

	case LIBRPC_USER_ACK:
		list_for_each_entry(cmpl, librpc_user_completions, list) {
			if (cmpl->portid == nsp->portid && cn->ack == cmpl->ack) {
				complete(cmpl->done);
				return;
			}
		}

		printk("librpc_user: spurious ack: portid=%u, seq=%u, ack=%u\n",
		    nsp->portid, cn->seq, cn->ack);
		return;
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

