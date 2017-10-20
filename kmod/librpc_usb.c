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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/usb.h>
#include "librpc.h"

struct librpc_usb_device;

static int librpc_usb_probe(struct usb_interface *, const struct usb_device_id *);
static void librpc_usb_disconnect(struct usb_interface *);
static int librpc_usb_enumerate(struct device *, struct librpc_endpoint *);
static int librpc_usb_ping(struct device *);
static int librpc_usb_request(struct device *, void *, const void *, size_t);
static int librpc_usb_xfer(struct usb_device *, int, void *, size_t, int);
static void librpc_usb_read_events(struct librpc_usb_device *);
static void librpc_usb_read_log(struct librpc_usb_device *);
static int librpc_usb_thread(void *);

#define	LIBRPC_MAX_MSGSIZE	4096

enum librpc_usb_opcode
{
        LIBRPC_USB_PING = 0,
        LIBRPC_USB_READ_ACK,
        LIBRPC_USB_IDENTIFY,
        LIBRPC_USB_SEND_REQ,
        LIBRPC_USB_READ_RESP,
        LIBRPC_USB_READ_EVENT,
    	LIBRPC_USB_READ_LOG
};

enum librpc_usb_status
{
        LIBRPC_USB_OK = 0,
        LIBRPC_USB_ERROR,
        LIBRPC_USB_NOT_READY,
	LIBRPC_USB_END
};

struct librpc_usb_response
{
        uint8_t         status;
        char            data[];
};

struct librpc_usb_identification
{
        uint16_t        iName;
    	uint16_t 	iDescription;
    	uint16_t 	log_size;
};

struct librpc_usb_log
{
    	uint8_t 	status;
    	char 		buffer[];
};

struct librpc_usb_device {
    	struct usb_device *		udev;
        struct librpc_device *  	rpcdev;
    	struct task_struct *		thread;
	struct librpc_usb_response *	event;
	struct librpc_usb_log *		log;
};

static struct librpc_ops librpc_usb_ops = {
        .open = NULL,
        .release = NULL,
        .enumerate = librpc_usb_enumerate,
        .ping = librpc_usb_ping,
        .request = librpc_usb_request
};

static int
librpc_usb_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
        struct librpc_usb_device *rpcusbdev;
        struct usb_device *udev = interface_to_usbdev(intf);
        struct librpc_endpoint endp;
        int ret;

        rpcusbdev = kzalloc(sizeof(*rpcusbdev), GFP_KERNEL);
        if (rpcusbdev == NULL)
                return (-ENOMEM);

        ret = librpc_usb_enumerate(&udev->dev, &endp);
        if (ret != 0) {
                kfree(rpcusbdev);
                return (ret);
        }

        usb_set_intfdata(intf, rpcusbdev);
	rpcusbdev->udev = udev;
        rpcusbdev->rpcdev = librpc_device_register("usb", &udev->dev,
            &librpc_usb_ops, THIS_MODULE);

	if (IS_ERR(rpcusbdev->rpcdev)) {
		kfree(rpcusbdev);
		return (PTR_ERR(rpcusbdev->rpcdev));
	}

	rpcusbdev->event = kmalloc(
	    sizeof(*rpcusbdev->event) + LIBRPC_MAX_MSGSIZE, GFP_KERNEL);
	rpcusbdev->log = kmalloc(
	    sizeof(*rpcusbdev->log) + LIBRPC_MAX_MSGSIZE, GFP_KERNEL);

	rpcusbdev->thread = kthread_run(&librpc_usb_thread, rpcusbdev, "librpc");
	return (0);
}

static void
librpc_usb_disconnect(struct usb_interface *intf)
{
        struct librpc_usb_device *rpcusbdev = usb_get_intfdata(intf);

	kthread_stop(rpcusbdev->thread);
	librpc_device_unregister(rpcusbdev->rpcdev);
}

static int
librpc_usb_enumerate(struct device *dev, struct librpc_endpoint *endp)
{
        struct usb_device *udev = to_usb_device(dev);
        struct librpc_usb_identification *ident;
        int ret = 0;


        ident = kmalloc(sizeof(*ident), GFP_KERNEL);
        ret = librpc_usb_xfer(udev, LIBRPC_USB_IDENTIFY, ident, sizeof(*ident), 500);
        if (ret < 0)
                goto done;

        ret = usb_string(udev, ident->iName, endp->name, NAME_MAX);
        if (ret < 0)
                goto done;

	printk("librpc_usb_enumerate: iName=%s, ret=%d\n", endp->name, ret);

        ret = usb_string(udev, ident->iDescription, endp->description, NAME_MAX);
        if (ret < 0)
                goto done;

	memcpy(endp->serial, udev->serial, strlen(udev->serial));
done:
        kfree(ident);
        return (ret < 0 ? ret : 0);
}

static int
librpc_usb_ping(struct device *dev)
{
        struct usb_device *udev = to_usb_device(dev);
        uint8_t *status;
        int ret;

	status = kmalloc(sizeof(*status), GFP_KERNEL);
        ret = librpc_usb_xfer(udev, LIBRPC_USB_PING, status, sizeof(uint8_t), 500);
        if (ret != 0) {
		kfree(status);
		return (ret);
	}

	ret = *status;
	kfree(status);
        return (ret);
}

static int
librpc_usb_request(struct device *dev, void *cookie, const void *buf, size_t len)
{
	struct usb_device *udev = to_usb_device(dev);
	struct librpc_usb_response *resp;
	int wpipe = usb_sndctrlpipe(udev, 0);
	int rpipe = usb_rcvctrlpipe(udev, 0);
	int ret;

	resp = kmalloc(sizeof(*resp) + LIBRPC_MAX_MSGSIZE, GFP_KERNEL);
	ret = usb_control_msg(udev, wpipe, LIBRPC_USB_SEND_REQ, USB_TYPE_VENDOR,
	    0, 0, (void *)buf, len, 500);

	if (ret < 0)
		return (ret);

	for (;;) {
		ret = usb_control_msg(udev, rpipe, LIBRPC_USB_READ_RESP,
		    USB_TYPE_VENDOR | USB_DIR_IN, 0, 0,
		    resp, sizeof(*resp) + LIBRPC_MAX_MSGSIZE, 500);

		if (ret < 0)
			return (ret);

		if (resp->status == LIBRPC_USB_NOT_READY) {
			msleep(10);
			continue;
		}

		librpc_device_answer(dev, cookie, resp->data, ret - 1);
		break;
	}

	return (resp->status);
}

static int
librpc_usb_xfer(struct usb_device *udev, int opcode, void *buf, size_t len,
    int timeout)
{
        int rpipe = usb_rcvctrlpipe(udev, 0);

	if (len > LIBRPC_MAX_MSGSIZE)
		return (-E2BIG);

        return (usb_control_msg(udev, rpipe, opcode,
            USB_TYPE_VENDOR | USB_DIR_IN, 0, 0, buf, len, timeout));
}

static void
librpc_usb_read_events(struct librpc_usb_device *rpcusbdev)
{
	struct usb_device *udev = rpcusbdev->udev;
	int rpipe = usb_rcvctrlpipe(udev, 0);
	int ret;

	for (;;) {
		ret = usb_control_msg(udev, rpipe, LIBRPC_USB_READ_EVENT,
		    USB_TYPE_VENDOR | USB_DIR_IN, 0, 0, rpcusbdev->event,
		    sizeof(*rpcusbdev->event) + LIBRPC_MAX_MSGSIZE, 500);

		if (ret < 0 || rpcusbdev->event->status != LIBRPC_USB_OK)
			return;

		librpc_device_event(&rpcusbdev->udev->dev,
		    rpcusbdev->event->data, ret);
	}
}

static void
librpc_usb_read_log(struct librpc_usb_device *rpcusbdev)
{
	struct usb_device *udev = rpcusbdev->udev;
	struct librpc_usb_log *log = rpcusbdev->log;
	int rpipe = usb_rcvctrlpipe(rpcusbdev->udev, 0);
	int ret;

	while (1) {
		ret = usb_control_msg(udev, rpipe, LIBRPC_USB_READ_LOG,
		    USB_TYPE_VENDOR | USB_DIR_IN, 0, 0, rpcusbdev->log,
		    sizeof(*rpcusbdev->log) + LIBRPC_MAX_MSGSIZE, 500);

		if (ret < 1)
			return;

		if (rpcusbdev->log->status != LIBRPC_USB_OK)
			break;

		librpc_device_log(&udev->dev, log->buffer, ret - 1);
	}
}

static int
librpc_usb_thread(void *arg)
{
	struct librpc_usb_device *rpcusbdev = arg;

	for (;;) {
		if (kthread_should_stop())
			break;

		//librpc_usb_read_events(rpcusbdev);
		librpc_usb_read_log(rpcusbdev);
		msleep(500);
	}

	return (0);
}

static struct usb_device_id librpc_usb_id_table[] = {
        { USB_DEVICE_INTERFACE_NUMBER(0xbeef, 0xfeed, 0x00) },
        { }
};

static struct usb_driver librpc_usb_driver = {
        .name = "librpc-usb",
        .probe = librpc_usb_probe,
        .disconnect = librpc_usb_disconnect,
        .id_table = librpc_usb_id_table
};

MODULE_AUTHOR("Jakub Klama");
MODULE_LICENSE("Dual BSD/GPL");

module_usb_driver(librpc_usb_driver);

