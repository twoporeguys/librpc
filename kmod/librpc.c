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

#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/workqueue.h>
#include <linux/connector.h>
#include <linux/stat.h>
#include "librpc.h"

static int librpc_bus_match(struct device *dev, struct device_driver *drv);
static void librpc_cn_callback(struct cn_msg *, struct netlink_skb_parms *);
static void librpc_dev_release(struct device *);
static struct device *librpc_find_device(uint32_t);
static ssize_t librpc_device_show_address(struct device *,
    struct device_attribute *, char *);
static ssize_t librpc_device_show_name(struct device *,
    struct device_attribute *, char *);
static ssize_t librpc_device_show_descr(struct device *,
    struct device_attribute *, char *);
static ssize_t librpc_device_show_serial(struct device *,
    struct device_attribute *, char *);
static void librpc_cn_send_ack(uint32_t, uint32_t, uint32_t, int);
static void librpc_cn_send_presence(int, uint32_t, struct librpc_endpoint *);
static void librpc_request(struct work_struct *);

struct librpc_call
{
	uint64_t                id;
	uint32_t                portid;
    	uint32_t 		seq;
    	uint32_t 		ack;
	struct librpc_device *	rpcdev;
	struct device *		dev;
	void *			data;
	size_t			len;
	struct work_struct	work;
};

struct librpc_dev
{
	struct device *         dev;
    	uint32_t		seq;
};

static struct cb_id librpc_cb_id = {
	.idx = CN_LIBRPC_IDX,
	.val = CN_LIBRPC_VAL
};

static struct bus_type librpc_bus_type = {
	.name = "librpc",
	.match = librpc_bus_match
};

static const struct device_attribute librpc_device_attrs[] = {
	__ATTR(address, S_IRUGO, librpc_device_show_address, NULL),
	__ATTR(name, S_IRUGO, librpc_device_show_name, NULL),
	__ATTR(description, S_IRUGO, librpc_device_show_descr, NULL),
	__ATTR(serial, S_IRUGO, librpc_device_show_serial, NULL)
};

static struct workqueue_struct *librpc_wq;
static struct class *librpc_class;
static struct librpc_dev *dev;
static DEFINE_MUTEX(librpc_mtx);
static DEFINE_IDR(librpc_device_ids);
static uint32_t resp_seq;

struct librpc_device *
librpc_device_register(const char *name, struct device *dev,
    const struct librpc_ops *ops, struct module *owner)
{
	struct librpc_device *rpcdev;
	int ret;
	int id;
	int i;

	mutex_lock(&librpc_mtx);
	rpcdev = kzalloc(sizeof(*rpcdev), GFP_KERNEL);
	id = idr_alloc(&librpc_device_ids, rpcdev, 0, 64, GFP_KERNEL);

	rpcdev->address = id;
	rpcdev->name = name;
	rpcdev->owner = owner;
	rpcdev->ops = ops;

	rpcdev->dev.parent = dev;
	rpcdev->dev.release = &librpc_dev_release;
	rpcdev->dev.bus = &librpc_bus_type;

	dev_set_name(&rpcdev->dev, "librpc%d", id);

	ret = ops->enumerate(dev, &rpcdev->endp);
	if (ret != 0) {
		dev_err(&rpcdev->dev, "cannot enumerate librpc endpoint: %d\n", ret);
		kfree(rpcdev);
		mutex_unlock(&librpc_mtx);
		return (ERR_PTR(ret));
	}

        ret = device_register(&rpcdev->dev);
	if (ret != 0) {
		kfree(rpcdev);
		mutex_unlock(&librpc_mtx);
		return (ERR_PTR(ret));
	}

	for (i = 0; i < ARRAY_SIZE(librpc_device_attrs); i++)
		device_create_file(&rpcdev->dev, &librpc_device_attrs[i]);

	dev_info(&rpcdev->dev, "new librpc endpoint: %s\n", rpcdev->endp.name);
	librpc_cn_send_presence(LIBRPC_ARRIVE, rpcdev->address, &rpcdev->endp);
	mutex_unlock(&librpc_mtx);
	return (rpcdev);
}

void
librpc_device_unregister(struct librpc_device *rpcdev)
{
	int i;

	mutex_lock(&librpc_mtx);
	for (i = 0; i < ARRAY_SIZE(librpc_device_attrs); i++)
		device_remove_file(&rpcdev->dev, &librpc_device_attrs[i]);

	device_unregister(&rpcdev->dev);
	idr_remove(&librpc_device_ids, rpcdev->address);
	librpc_cn_send_presence(LIBRPC_DEPART, rpcdev->address, &rpcdev->endp);
	kfree(rpcdev);
	mutex_unlock(&librpc_mtx);
}

void
librpc_device_answer(struct device *dev, void *arg, const void *buf,
    size_t length)
{
	struct librpc_call *call = arg;
	struct {
	    struct cn_msg cn;
	    struct librpc_message msg;
	    char response[length];
	} packet;

	printk("librpc_device_answer: buf=%p, length=%zu\n", buf, sizeof(packet));
	print_hex_dump(KERN_INFO, "response: ", DUMP_PREFIX_ADDRESS, 16,
	    1, buf, length, true);

	packet.cn.id = librpc_cb_id;
	packet.cn.seq = resp_seq++;
	packet.cn.ack = 0;
	packet.cn.len = sizeof(packet);
	packet.cn.flags = 0;
	packet.msg.opcode = LIBRPC_RESPONSE;
	packet.msg.status = 0;

	memcpy(packet.response, buf, length);
	cn_netlink_send(&packet.cn, call->portid, 0, GFP_KERNEL);
}

void
librpc_device_error(struct device *dev, void *arg, int error)
{
	struct librpc_call *call = arg;
	struct {
	    struct cn_msg cn;
	    struct librpc_message msg;
	} packet;

	printk("librpc_device_error: error=%d\n", error);

	packet.cn.id = librpc_cb_id;
	packet.cn.seq = resp_seq++;
	packet.cn.ack = 0;
	packet.cn.len = sizeof(packet);
	packet.cn.flags = 0;
	packet.msg.opcode = LIBRPC_RESPONSE;
	packet.msg.status = error;

	cn_netlink_send(&packet.cn, call->portid, 0, GFP_KERNEL);
}

void
librpc_device_event(struct device *dev, const void *buf, size_t length)
{
	struct {
		struct cn_msg cn;
		struct librpc_message msg;
		char response[length];
	} packet;

	printk("librpc_device_event: device=%p\n", dev);

	packet.cn.id = librpc_cb_id;
	packet.cn.seq = resp_seq++;
	packet.cn.ack = 0;
	packet.cn.len = sizeof(packet);
	packet.cn.flags = 0;
	packet.msg.opcode = LIBRPC_EVENT;
	packet.msg.status = 0;

	memcpy(packet.response, buf, length);
	cn_netlink_send(&packet.cn, 0, 0, GFP_KERNEL);
}

void
librpc_device_log(struct device *dev, const char *log, size_t len)
{

	dev_dbg(dev, "librpc_device_log: len=%zu, buf=%p\n", len, log);
	dev_info(dev, "%*s\n", (int)len, log);
}

int
librpc_ping(uint32_t address)
{
	struct librpc_device *rpcdev;
	struct device *dev;

	dev = librpc_find_device(address);
	if (dev == NULL)
		return (-ENOENT);

	rpcdev = to_librpc_device(dev);
	return (rpcdev->ops->ping(dev->parent));
}

static int
librpc_bus_match(struct device *dev, struct device_driver *drv)
{

	return (0);
}

static int
librpc_match_device(struct device *dev, void *data)
{
	struct librpc_device *rpcdev = to_librpc_device(dev);
	uint32_t *address = data;

	return (rpcdev->address == *address);
}

static struct device *
librpc_find_device(uint32_t address)
{

	return (bus_find_device(&librpc_bus_type, NULL, &address,
	    librpc_match_device));
}

static void
librpc_request(struct work_struct *work)
{
	struct librpc_call *call = container_of(work, struct librpc_call, work);
	int ret;

	ret = call->rpcdev->ops->request(call->dev, call, call->data,
            call->len);
}

static void
librpc_dev_release(struct device *dev)
{

}

static void
librpc_cn_callback(struct cn_msg *cn, struct netlink_skb_parms *nsp)
{
	struct librpc_message *msg = (struct librpc_message *)(cn + 1);
	struct librpc_call *call;
	struct librpc_device *rpcdev;
	struct device *dev;
	int ret = 0;

	printk("librpc_cn_callback: msg: opcode=%d, address=0x%08x, len=%d, "
	    "seq=%d, portid=%d",  msg->opcode, msg->address, cn->len,
	    cn->seq, nsp->portid);

	switch (msg->opcode) {
	case LIBRPC_QUERY:
		break;

	case LIBRPC_PING:
		dev = librpc_find_device(msg->address);
		if (dev == NULL) {
			ret = ENOENT;
			goto ack;
		}

		rpcdev = to_librpc_device(dev);
		ret = rpcdev->ops->ping(dev->parent);
		break;

	case LIBRPC_REQUEST:
		dev = librpc_find_device(msg->address);
		if (dev == NULL) {
			ret = ENOENT;
			goto ack;
		}

		call = kzalloc(sizeof(*call), GFP_KERNEL);
		call->rpcdev = to_librpc_device(dev);
		call->dev = dev->parent;
		call->len = cn->len - sizeof(*msg);
		call->data = kmalloc(call->len, GFP_KERNEL);
		call->portid = nsp->portid;
		call->seq = cn->seq;
		call->ack = cn->ack;
		call->id = cn->seq;

		memcpy(call->data, msg->data, call->len);
		INIT_WORK(&call->work, &librpc_request);
		queue_work(librpc_wq, &call->work);
		break;
	}

ack:
	librpc_cn_send_ack(cn->seq, cn->ack, nsp->portid, ret);
}

static void
librpc_cn_send_ack(uint32_t seq, uint32_t ack, uint32_t portid, int error)
{
	int ret;
	struct {
		struct cn_msg cn;
		struct librpc_message msg;
	} packet;

	printk("librpc_cn_send_ack: seq=%d, portid=%d, status=%d\n",
	    seq, portid, error);

	packet.cn.id = librpc_cb_id;
	packet.cn.seq = seq;
	packet.cn.ack = ack + 1;
	packet.cn.len = sizeof(packet);
	packet.cn.flags = 0;
	packet.msg.opcode = LIBRPC_ACK;
	packet.msg.status = error;

	ret = cn_netlink_send(&packet.cn, portid, 0, GFP_KERNEL);
	if (ret < 0)
		printk("librpc_cn_send_ack: send failed, err=%d\n", ret);
}

static void
librpc_cn_send_presence(int opcode, uint32_t address,
    struct librpc_endpoint *endpoint)
{
	struct {
		struct cn_msg cn;
		struct librpc_message msg;
		struct librpc_endpoint endp;
	} packet;

	printk("librpc_cn_send_presence: opcode=%d, epname=%s\n",
	    opcode, endpoint->name);

	packet.cn.id = librpc_cb_id;
	packet.cn.seq = resp_seq++;
	packet.cn.ack = 0;
	packet.cn.len = sizeof(packet);
	packet.cn.flags = 0;
	packet.msg.opcode = opcode;
	packet.msg.address = address;
	packet.msg.status = 0;
	packet.endp = *endpoint;

	cn_netlink_send(&packet.cn, 0, 0, GFP_KERNEL);
}

static ssize_t
librpc_device_show_address(struct device *dev, struct device_attribute *attr,
    char *buf)
{
	struct librpc_device *rpcdev = to_librpc_device(dev);

	return (sprintf(buf, "%u\n", rpcdev->address));
}

static ssize_t
librpc_device_show_name(struct device *dev, struct device_attribute *attr,
    char *buf)
{
	struct librpc_device *rpcdev = to_librpc_device(dev);

	return (sprintf(buf, "%s\n", rpcdev->endp.name));
}

static ssize_t
librpc_device_show_descr(struct device *dev, struct device_attribute *attr,
    char *buf)
{
	struct librpc_device *rpcdev = to_librpc_device(dev);

	return (sprintf(buf, "%s\n", rpcdev->endp.description));
}

static ssize_t
librpc_device_show_serial(struct device *dev, struct device_attribute *attr,
    char *buf)
{
	struct librpc_device *rpcdev = to_librpc_device(dev);

	return (sprintf(buf, "%s\n", rpcdev->endp.serial));
}

static int
librpc_device_destroy(struct device *dev, void *data)
{

	librpc_device_unregister(to_librpc_device(dev));
	return (0);
}

static int __init
librpc_init(void)
{
	int ret;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (dev == NULL)
		return (-ENOMEM);

	ret = bus_register(&librpc_bus_type);
	if (ret != 0)
		goto done;

	librpc_class = class_create(THIS_MODULE, "librpc");
	if (IS_ERR(librpc_class)) {
		ret = PTR_ERR(librpc_class);
		goto done;
	}

	dev->dev = device_create(librpc_class, NULL, MKDEV(0, 0), NULL, "librpc");
	if (IS_ERR(dev->dev)) {
		ret = PTR_ERR(dev->dev);
		goto done;
	}

	librpc_wq = alloc_workqueue("librpc-wq", 0, 0);
	if (librpc_wq == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	ret = cn_add_callback(&librpc_cb_id, "librpc", librpc_cn_callback);
	if (ret != 0)
		goto done;

done:
	kfree(dev);
	return (ret);
}

static void __exit
librpc_exit(void)
{
	cn_del_callback(&librpc_cb_id);
	bus_for_each_dev(&librpc_bus_type, NULL, NULL, librpc_device_destroy);
	device_destroy(librpc_class, MKDEV(0, 0));
	class_destroy(librpc_class);
	bus_unregister(&librpc_bus_type);
}

EXPORT_SYMBOL(librpc_device_register);
EXPORT_SYMBOL(librpc_device_unregister);
EXPORT_SYMBOL(librpc_device_answer);
EXPORT_SYMBOL(librpc_device_error);
EXPORT_SYMBOL(librpc_device_event);
EXPORT_SYMBOL(librpc_device_log);
MODULE_AUTHOR("Jakub Klama <jakub.klama@twoporeguys.com>");
MODULE_LICENSE("Dual BSD/GPL");

module_init(librpc_init);
module_exit(librpc_exit);
