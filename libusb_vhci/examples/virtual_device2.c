/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2009-2019 Michael Singer <michael@a-singer.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This example shows how to create a virtual usb host controller with
 * a virtual device plugged into it. If you want to have it running, you
 * have to do the following:
 *
 * 1. Download, build and load the kernel modules. You can download them here:
 *    http://sourceforge.net/projects/usb-vhci/files/linux%20kernel%20module/
 *    (See README or INSTALL instructions in its source directory.)
 * 2. Build the libraries and this example. ("./configure && make" in
 *    the top level directory of this source package.)
 * 3. Run "./virtual_device2" in the examples subdirectory. You have to
 *    be root, if you did not make /dev/usb-vhci accessible for all
 *    users (chmod 666 /dev/usb-vhci).
 *
 * Now type "dmesg" in another shell or "cat /proc/bus/usb/devices" if
 * you have usbfs mounted. You should see a dummy usb device, which does
 * nothing else than answering a few control requests from the usb core.
 */

#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include "../src/libusb_vhci.h"

const uint8_t dev_desc[] = {
	18,     // descriptor length
	1,      // type: device descriptor
	0x00,   // bcd usb release number
	0x02,   //  "
	0,      // device class: per interface
	0,      // device sub class
	0,      // device protocol
	64,     // max packet size
	0xad,   // vendor id
	0xde,   //  "
	0xef,   // product id
	0xbe,   //  "
	0x38,   // bcd device release number
	0x11,   //  "
	0,      // manufacturer string
	1,      // product string
	0,      // serial number string
	1       // number of configurations
};

const uint8_t conf_desc[] = {
	9,      // descriptor length
	2,      // type: configuration descriptor
	18,     // total descriptor length (configuration+interface)
	0,      //  "
	1,      // number of interfaces
	1,      // configuration index
	0,      // configuration string
	0x80,   // attributes: none
	0,      // max power

	9,      // descriptor length
	4,      // type: interface
	0,      // interface number
	0,      // alternate setting
	0,      // number of endpoints
	0,      // interface class
	0,      // interface sub class
	0,      // interface protocol
	0       // interface string
};

const uint8_t str0_desc[] = {
	4,      // descriptor length
	3,      // type: string
	0x09,   // lang id: english (us)
	0x04    //  "
};

const uint8_t *str1_desc = (uint8_t *)"\x1a\x03H\0e\0l\0l\0o\0 \0W\0o\0r\0l\0d\0!";

void process_urb(struct usb_vhci_urb *urb)
{
	if(!usb_vhci_is_control(urb->type))
	{
		printf("not CONTROL\n");
		return;
	}
	if(urb->epadr & 0x7f)
	{
		printf("not ep0\n");
		urb->status = USB_VHCI_STATUS_STALL;
		return;
	}

	uint8_t rt = urb->bmRequestType;
	uint8_t r = urb->bRequest;
	if(rt == 0x00 && r == URB_RQ_SET_CONFIGURATION)
	{
		printf("SET_CONFIGURATION\n");
		urb->status = USB_VHCI_STATUS_SUCCESS;
	}
	else if(rt == 0x00 && r == URB_RQ_SET_INTERFACE)
	{
		printf("SET_INTERFACE\n");
		urb->status = USB_VHCI_STATUS_SUCCESS;
	}
	else if(rt == 0x80 && r == URB_RQ_GET_DESCRIPTOR)
	{
		printf("GET_DESCRIPTOR\n");
		int l = urb->wLength;
		uint8_t *buffer = urb->buffer;
		switch(urb->wValue >> 8)
		{
		case 1:
			printf("DEVICE_DESCRIPTOR\n");
			if(dev_desc[0] < l) l = dev_desc[0];
			memcpy(buffer, dev_desc, l);
			urb->buffer_actual = l;
			urb->status = USB_VHCI_STATUS_SUCCESS;
			break;
		case 2:
			printf("CONFIGURATION_DESCRIPTOR\n");
			if(conf_desc[2] < l) l = conf_desc[2];
			memcpy(buffer, conf_desc, l);
			urb->buffer_actual = l;
			urb->status = USB_VHCI_STATUS_SUCCESS;
			break;
		case 3:
			printf("STRING_DESCRIPTOR\n");
			switch(urb->wValue & 0xff)
			{
			case 0:
				if(str0_desc[0] < l) l = str0_desc[0];
				memcpy(buffer, str0_desc, l);
				urb->buffer_actual = l;
				urb->status = USB_VHCI_STATUS_SUCCESS;
				break;
			case 1:
				if(str1_desc[0] < l) l = str1_desc[0];
				memcpy(buffer, str1_desc, l);
				urb->buffer_actual = l;
				urb->status = USB_VHCI_STATUS_SUCCESS;
				break;
			default:
				urb->status = USB_VHCI_STATUS_STALL;
				break;
			}
		default:
			urb->status = USB_VHCI_STATUS_STALL;
			break;
		}
	}
	else
		urb->status = USB_VHCI_STATUS_STALL;
}

int main()
{
	int32_t id, usb_bus_num;
	char *bus_id = NULL; // will be allocated by usb_vhci_open; use free(bus_id) to free it, if you want

	// create a host controller with only one port
	int fd = usb_vhci_open(1, &id, &usb_bus_num, &bus_id);
	printf("created %s (bus# %d)\n", bus_id, usb_bus_num);

	// contains the status of the port
	struct usb_vhci_port_stat stat;
	memset(&stat, 0, sizeof stat);

	// contains the address of our device connected to the port
	// (the device is not yet connected)
	uint8_t adr = 0xff; // address not set yet

	while(true)
	{
		struct usb_vhci_work w;
		int res = usb_vhci_fetch_work(fd, &w);
		if(res == -1)
		{
			if(errno != ETIMEDOUT && errno != EINTR && errno != ENODATA)
				fprintf(stderr, "usb_vhci_fetch_work failed with errno %d", errno);
		}
		else
		{
			uint16_t status, change;
			uint8_t flags, index;
			switch(w.type)
			{
			case USB_VHCI_WORK_TYPE_PORT_STAT:
				status = w.work.port_stat.status;
				change = w.work.port_stat.change;
				flags = w.work.port_stat.flags;
				index = w.work.port_stat.index;
				printf("got port stat work\n");
				printf("status: 0x%04hx\n", status);
				printf("change: 0x%04hx\n", change);
				printf("flags:  0x%02hhx\n", flags);
				if(index != 1)
				{
					fprintf(stderr, "invalid port %hhu\n", index);
					return 1;
				}
				struct usb_vhci_port_stat prev = stat;
				stat = w.work.port_stat;
				if(change & USB_VHCI_PORT_STAT_C_CONNECTION)
				{
					printf("CONNECTION state changed -> invalidating address\n");
					adr = 0xff;
				}
				if(change & USB_VHCI_PORT_STAT_C_RESET && ~status & USB_VHCI_PORT_STAT_RESET && status & USB_VHCI_PORT_STAT_ENABLE)
				{
					printf("RESET successfull -> use default address\n");
					adr = 0;
				}
				if(prev.status & USB_VHCI_PORT_STAT_POWER && ~status & USB_VHCI_PORT_STAT_POWER)
				{
					printf("port is powered off\n");
				}
				if(~prev.status & USB_VHCI_PORT_STAT_POWER && status & USB_VHCI_PORT_STAT_POWER)
				{
					printf("port is powered on -> connecting device\n");
					if(usb_vhci_port_connect(fd, 1, USB_VHCI_DATA_RATE_FULL) == -1)
					{
						fprintf(stderr, "usb_vhci_port_connect failed with errno %d\n", errno);
						return 1;
					}
				}
				if(~prev.status & USB_VHCI_PORT_STAT_RESET && status & USB_VHCI_PORT_STAT_RESET)
				{
					printf("port is resetting\n");
					if(status & USB_VHCI_PORT_STAT_CONNECTION)
					{
						printf("-> completing reset\n");
						if(usb_vhci_port_reset_done(fd, 1, 1) == -1)
						{
							fprintf(stderr, "usb_vhci_port_reset_done failed with errno %d\n", errno);
							return 1;
						}
					}
				}
				if(~prev.flags & USB_VHCI_PORT_STAT_FLAG_RESUMING && flags & USB_VHCI_PORT_STAT_FLAG_RESUMING)
				{
					printf("port is resuming\n");
					if(status & USB_VHCI_PORT_STAT_CONNECTION)
					{
						printf("-> completing resume\n");
						if(usb_vhci_port_resumed(fd, 1) == -1)
						{
							fprintf(stderr, "usb_vhci_port_resumed failed with errno %d\n", errno);
							return 1;
						}
					}
				}
				if(~prev.status & USB_VHCI_PORT_STAT_SUSPEND && status & USB_VHCI_PORT_STAT_SUSPEND)
					printf("port is suspended\n");
				if(prev.status & USB_VHCI_PORT_STAT_ENABLE && ~status & USB_VHCI_PORT_STAT_ENABLE)
					printf("port is disabled\n");
				break;
			case USB_VHCI_WORK_TYPE_PROCESS_URB:
				printf("got process urb work\n");
				w.work.urb.buffer = NULL;
				w.work.urb.iso_packets = NULL;
				if(w.work.urb.devadr != adr)
				{
					// not for me
					break;
				}
				if(w.work.urb.buffer_length)
					w.work.urb.buffer = (uint8_t *)malloc(w.work.urb.buffer_length);
				if(w.work.urb.packet_count)
					w.work.urb.iso_packets = (struct usb_vhci_iso_packet *)malloc(w.work.urb.packet_count * sizeof(struct usb_vhci_iso_packet));
				if(res) // usb_vhci_fetch_work has returned a value != 0
				{
					res = usb_vhci_fetch_data(fd, &w.work.urb);
					if(res == -1)
					{
						if(errno != ECANCELED)
							fprintf(stderr, "usb_vhci_fetch_data failed with errno %d", errno);
						free(w.work.urb.buffer);
						free(w.work.urb.iso_packets);
					}
				}
				// SET_ADDRESS?
				if(usb_vhci_is_control(w.work.urb.type) && !(w.work.urb.epadr & 0x7f) && !w.work.urb.bmRequestType && w.work.urb.bRequest == 5)
				{
					if(w.work.urb.wValue > 0x7f)
						w.work.urb.status = USB_VHCI_STATUS_STALL;
					else
					{
						w.work.urb.status = USB_VHCI_STATUS_SUCCESS;
						adr = (uint8_t)w.work.urb.wValue;
						printf("SET_ADDRESS (adr=%hhu)\n", adr);
					}
				}
				// any other than SET_ADDRESS?
				else
				{
					process_urb(&w.work.urb);
				}
				if(usb_vhci_giveback(fd, &w.work.urb) == -1)
					fprintf(stderr, "usb_vhci_giveback failed with errno %d", errno);
				free(w.work.urb.buffer);
				free(w.work.urb.iso_packets);
				break;
			case USB_VHCI_WORK_TYPE_CANCEL_URB:
				printf("got cancel urb work\n");
				break;
			default:
				fprintf(stderr, "got invalid work\n");
				return 1;
			}
		}
	}

	return 0;
}

