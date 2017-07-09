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
#include <linux/init.h>
#include <linux/usb.h>

static int librpc_usb_probe(struct usb_interface *, const struct usb_device_id *);
static void librpc_usb_disconnect(struct usb_interface *);
static int librpc_usb_enumerate(struct device *, struct librpc_endpoint *);
static int librpc_usb_ping(struct device *, struct librpc_endpoint *);
static int librpc_usb_request(struct device *, uint64_t, const void *, size_t);

enum librpc_usb_opcode {
        LIBRPC_USB_PING = 0,
        LIBRPC_USB_READ_ACK,
        LIBRPC_USB_IDENTIFY,
        LIBRPC_USB_SEND_REQ,
        LIBRPC_USB_READ_RESP,
        LIBRPC_USB_READ_EVENTS
};

enum librpc_usb_status {
        LIBRPC_USB_OK = 0,
        LIBRPC_USB_ERROR,
        LIBRPC_USB_NOT_READY
};

struct librpc_usb_response {
        uint8_t         status;
        char            data[];
};

struct librpc_usb_identification {
        uint15_t        iName;
        uint16_t        iSerial;
};

struct librpc_usb_device {
        struct librpc_device *  rpcdev;
}

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
        struct usb_device *udev = interface_to_usbdev(intf);

        librpc_device_register("usb", udev->dev, &librpc_usb_ops, THIS_MODULE);
}

static void
librpc_usb_disconnect(struct usb_interface *intf)
{
        /* code */
}

static int
librpc_usb_enumerate(struct device *dev, struct librpc_endpoint *endp)
{
        struct librpc_usb_identification ident;
        int ret;

        ret = usb_control_msg(udev, LIBRPC_USB_IDENTIFY, &ident, sizeof(ident), 500);
        if (ret != 0)
                return (ret);

        ret = usb_string(ident->iName, endp->name, NAME_MAX);
        if (ret != 0)
                return (ret);

        ret = usb_string(ident->iSerial, endp->serial, NAME_MAX);
        if (ret != 0)
                return (ret);

        return (0);
}

static int
librpc_usb_ping(struct device *dev)
{
        uint8_t status;
        int ret;

        ret = usb_control_msg(udev, LIBRPC_USB_PING, &status, sizeof(uint8_t), 500);
        if (ret != 0)
                return (ret);

        return (status);
}

static int
librpc_usb_request(struct device *dev, uint64_t id, const void *buf, size_t len)
{
        int wpipe = usb_sndctrlpipe(udev, 0);
        int rpipe = usb_rcvctrlpipe(udev, 0);

        usb_control_msg(udev, wpipe, )
}

static int
librpc_usb_xfer(struct usb_device *udev, int opcode, void *buf, size_t len,
    int timeout)
{
        int rpipe = usb_rcvctrlpipe(udev, 1);

        return (usb_control_msg(udev, rpipe, opcode, USB_TYPE_VENDOR, 0, 0, buf,
            len, timeout));
}

static int
librpc_usb_req(struct usb_interface *intf, int opcode, void *in, size_t ilen,
    void *out, size_t olen, int timeout)
{
        struct librpc_usb_response *resp;
        int wpipe = usb_sndctrlpipe(udev, 0);
        int rpipe = usb_rcvctrlpipe(udev, 0);
        int ret;

        ret = usb_control_msg(udev, wpipe, opcode, USB_TYPE_VENDOR, 0, 0, in,
            ilen, timeout);
        if (ret != 0)
                return (ret);

        for (;;) {
                ret = usb_control_msg(udev, rpipe, LIBRPC_USB_READ_ACK,
                    USB_TYPE_VENDOR, 0, 0, out, olen, timeout);
                if (ret != 0)
                        return (ret);

                resp = out;

                if (resp->status == LIBRPC_USB_NOT_READY)
                        continue;
        }

        return (resp->status);
}

static struct usb_device_id librpc_usb_id_table[] = {
        { USB_DEVICE_AND_INTERFACE_INFO(0xbeef, 0xfed, 0xff, 0x01, 0x01) },
        { }
};

static struct usb_driver librpc_usb_driver = {
        .name = "librpc-usb",
        .probe = librpc_usb_probe,
        .disconnect = librpc_usb_disconnect,
        .id_table = librpc_usb_id_table
};

module_usb_driver(librpc_usb_driver);
