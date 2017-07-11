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
#include <linux/connector.h>
#include <linux/stat.h>
#include "librpc.h"

static int librpc_bus_match(struct device *dev, struct device_driver *drv);
static void librpc_cn_callback(struct cn_msg *, struct netlink_skb_parms *);
static struct device *librpc_find_device(uint32_t);
static ssize_t librpc_device_show_address(struct device *,
    struct device_attribute *, char *);
static ssize_t librpc_device_show_name(struct device *,
    struct device_attribute *, char *);
static ssize_t librpc_device_show_descr(struct device *,
    struct device_attribute *, char *);
static ssize_t librpc_device_show_serial(struct device *,
    struct device_attribute *, char *);

struct librpc_call
{
        uint64_t                id;
        uint32_t                portid;
};

struct librpc_dev
{
        struct device *         dev;
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

static struct class *librpc_class;
static struct librpc_dev *dev;
static dev_t librpc_devid;
static DEFINE_MUTEX(librpc_mtx);
static DEFINE_IDR(librpc_device_ids);

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
        rpcdev->dev.bus = &librpc_bus_type;

        dev_set_name(&rpcdev->dev, "librpc%d", id);
        ret = device_register(&rpcdev->dev);
        if (ret != 0) {
                kfree(rpcdev);
                mutex_unlock(&librpc_mtx);
                return (ERR_PTR(ret));
        }

        for (i = 0; i < ARRAY_SIZE(librpc_device_attrs); i++)
                device_create_file(&rpcdev->dev, &librpc_device_attrs[i]);

        ops->enumerate(dev, &rpcdev->endp);
        mutex_unlock(&librpc_mtx);
        return (rpcdev);
}

void
librpc_device_unregister(struct librpc_device *rpcdev)
{

}

void
librpc_device_answer(struct device *dev, uint64_t id, const void *buf,
    size_t length)
{

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
        uint32_t address = (uint32_t)data;

        return (0);
}

static struct device *
librpc_find_device(uint32_t address)
{

        return (bus_find_device(&librpc_bus_type, NULL, (void *)address,
            librpc_match_device));
}

static void
librpc_cn_callback(struct cn_msg *cn, struct netlink_skb_parms *nsp)
{
        struct librpc_message *msg = (struct librpc_message *)(cn + 1);
        struct librpc_device *rpcdev;
        struct device *dev;
        int ret;

        switch (msg->opcode) {
        case LIBRPC_QUERY:
                break;

        case LIBRPC_PING:
                dev = librpc_find_device(msg->address);
                if (dev == NULL)
                        return (-ENOENT);

                rpcdev = to_librpc_device(dev);
                ret = rpcdev->ops->ping(dev->parent);

        case LIBRPC_REQUEST:
                dev = librpc_find_device(msg->address);
                if (dev == NULL)
                        return (-ENOENT);

                rpcdev = to_librpc_device(dev);
                //ret = rpcdev->ops->request(dev->parent, req);
                break;
        }
}

static void
librpc_cn_send_ack(uint32_t seq, uint32_t portid, int error)
{
        struct {
                struct cn_msg cn;
                struct librpc_message msg;
        } packet;

        packet.cn.id = librpc_cb_id;
        packet.cn.seq = seq;
        packet.cn.len = sizeof(struct librpc_message);
        packet.msg.opcode = LIBRPC_ACK;
        packet.msg.status = error;
        cn_netlink_send(&packet.cn, portid, CN_LIBRPC_IDX, GFP_KERNEL);
}

static void
librpc_cn_send_response(void)
{
        struct {
                struct cn_msg cn;
                struct librpc_message msg;
        } packet;
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

static int __init
librpc_init(void)
{
        dev_t devid;
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

        dev->dev = device_create(librpc_class, NULL, devid, NULL, "librpc");
        if (IS_ERR(dev->dev)) {
                ret = PTR_ERR(dev->dev);
                goto done;
        }

        ret = cn_add_callback(&librpc_cb_id, "librpc", &librpc_cn_callback);
        if (ret != 0)
                goto done;

done:
        kfree(dev);
        return (ret);
}

static void
librpc_exit(void)
{
        device_destroy(librpc_class, MKDEV(MAJOR(librpc_devid), 1));
        class_destroy(librpc_class);
}

EXPORT_SYMBOL(librpc_device_register);
EXPORT_SYMBOL(librpc_device_unregister);
MODULE_AUTHOR("Jakub Klama");
MODULE_LICENSE("Dual BSD/GPL");

module_init(librpc_init);
module_exit(librpc_exit);
