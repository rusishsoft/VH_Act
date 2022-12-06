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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "libusb_vhci.h"

int usb_vhci_open(uint8_t port_count,  // [IN]  number of ports
                  int32_t *id,         // [OUT] controller id
                  int32_t *usb_busnum, // [OUT] usb bus number
                  char    **bus_id)    // [OUT] bus id (usually
                                       //       vhci_hcd.<controller id>)
{
	int fd = open(USB_VHCI_DEVICE_FILE, O_RDWR);
	if(fd == -1) return -1;

	struct usb_vhci_ioc_register r;
	r.port_count = port_count;
	if(ioctl(fd, USB_VHCI_HCD_IOCREGISTER, &r) == -1)
	{
		int err = errno;
		usb_vhci_close(fd);
		errno = err;
		return -1;
	}

	if(id) *id = r.id;
	if(usb_busnum) *usb_busnum = r.usb_busnum;
	if(bus_id)
	{
		size_t s = sizeof r.bus_id / sizeof *r.bus_id - 1;
		r.bus_id[s] = 0;
		size_t ss = (strlen(r.bus_id) + 1) * sizeof **bus_id;
		if((*bus_id = malloc(ss)))
			memcpy(*bus_id, r.bus_id, ss);
	}

	return fd;
}

int usb_vhci_close(int fd)
{
	int result;
	while((result = close(fd)) == -1 && errno == EINTR);
	return result;
}

int usb_vhci_fetch_work(int fd, struct usb_vhci_work *work)
{
	return usb_vhci_fetch_work_timeout(fd, work, 100);
}

int usb_vhci_fetch_work_timeout(int fd, struct usb_vhci_work *work, int16_t timeout)
{
	struct usb_vhci_ioc_work w;
	w.timeout = timeout;
	if(ioctl(fd, USB_VHCI_HCD_IOCFETCHWORK, &w) == -1)
		return -1;

	switch(w.type)
	{
	case USB_VHCI_WORK_TYPE_PORT_STAT:
		work->type = USB_VHCI_WORK_TYPE_PORT_STAT;
		work->work.port_stat.status = w.work.port.status;
		work->work.port_stat.change = w.work.port.change;
		work->work.port_stat.index  = w.work.port.index;
		work->work.port_stat.flags  = w.work.port.flags;
		return 0;

	case USB_VHCI_WORK_TYPE_PROCESS_URB:
		memset(&work->work.urb, 0, sizeof work->work.urb);
		switch(w.work.urb.type)
		{
		case USB_VHCI_URB_TYPE_ISO:
			work->work.urb.packet_count  = w.work.urb.packet_count;
		case USB_VHCI_URB_TYPE_INT:
			work->work.urb.interval      = w.work.urb.interval;
			break;
		case USB_VHCI_URB_TYPE_CONTROL:
			work->work.urb.wValue        = w.work.urb.setup_packet.wValue;
			work->work.urb.wIndex        = w.work.urb.setup_packet.wIndex;
			work->work.urb.wLength       = w.work.urb.setup_packet.wLength;
			work->work.urb.bmRequestType = w.work.urb.setup_packet.bmRequestType;
			work->work.urb.bRequest      = w.work.urb.setup_packet.bRequest;
			break;
		case USB_VHCI_URB_TYPE_BULK:
			work->work.urb.flags         = w.work.urb.flags &
			                               (USB_VHCI_URB_FLAGS_SHORT_NOT_OK |
			                                USB_VHCI_URB_FLAGS_ZERO_PACKET);
			break;
		default:
			errno = EBADMSG;
			return -1;
		}
		work->type = USB_VHCI_WORK_TYPE_PROCESS_URB;
		work->work.urb.type          = w.work.urb.type;
		work->work.urb.status        = USB_VHCI_STATUS_PENDING;
		work->work.urb.handle        = w.handle;
		work->work.urb.buffer_length = w.work.urb.buffer_length;
		if(usb_vhci_is_out(w.work.urb.endpoint) || usb_vhci_is_iso(work->work.urb.type))
			work->work.urb.buffer_actual = w.work.urb.buffer_length;
		work->work.urb.devadr        = w.work.urb.address;
		work->work.urb.epadr         = w.work.urb.endpoint;
		// return 1 if usb_vhci_fetch_data should be called
		return work->work.urb.buffer_actual || work->work.urb.packet_count;

	case USB_VHCI_WORK_TYPE_CANCEL_URB:
		work->type = USB_VHCI_WORK_TYPE_CANCEL_URB;
		work->work.handle = w.handle;
		return 0;

	default:
		errno = EBADMSG;
		return -1;
	}
}

int usb_vhci_fetch_data(int fd, const struct usb_vhci_urb *urb)
{
	struct usb_vhci_ioc_urb_data u;
	u.handle        = urb->handle;
	u.buffer_length = urb->buffer_length;
	const int pc    = urb->packet_count;
	u.packet_count  = pc;
	u.buffer        = urb->buffer;
	u.iso_packets   = NULL;
	if(pc > 0)
		u.iso_packets = malloc(sizeof *u.iso_packets * pc);

	int ret = ioctl(fd, USB_VHCI_HCD_IOCFETCHDATA, &u);
	if(ret == -1)
		goto err;
	ret = 0;
	for(int i = 0; i < pc; i++)
	{
		urb->iso_packets[i].offset = u.iso_packets[i].offset;
		urb->iso_packets[i].packet_length = (int32_t)u.iso_packets[i].packet_length;
		urb->iso_packets[i].packet_actual = 0;
		urb->iso_packets[i].status = USB_VHCI_STATUS_PENDING;
	}

err:
	if(u.iso_packets)
		free(u.iso_packets);
	return ret;
}

int usb_vhci_giveback(int fd, const struct usb_vhci_urb *urb)
{
	struct usb_vhci_ioc_giveback gb;
	gb.handle = urb->handle;
	gb.status = usb_vhci_to_errno(urb->status, usb_vhci_is_iso(urb->type));
	gb.buffer_actual = urb->buffer_actual;
	gb.buffer = NULL;
	gb.iso_packets = NULL;
	gb.packet_count = 0;
	gb.error_count = 0;

	if(usb_vhci_is_in(urb->epadr) && gb.buffer_actual > 0)
		gb.buffer = urb->buffer;
	if(usb_vhci_is_iso(urb->type))
	{
		const int pc = urb->packet_count;
		gb.iso_packets = malloc(sizeof *gb.iso_packets * pc);
		gb.packet_count = pc;
		gb.error_count = urb->error_count;
		for(int i = 0; i < pc; i++)
		{
			gb.iso_packets[i].status = usb_vhci_to_iso_packets_errno(urb->iso_packets[i].status);
			gb.iso_packets[i].packet_actual = (uint32_t)urb->iso_packets[i].packet_actual;
		}
	}

	int ret = ioctl(fd, USB_VHCI_HCD_IOCGIVEBACK, &gb);

	if(gb.iso_packets)
		free(gb.iso_packets);
	if(ret == -1)
		return (errno == ECANCELED) ? 0 : -1;
	errno = 0;
	return 0;
}

int usb_vhci_port_connect(int fd, uint8_t port, uint8_t data_rate)
{
	if(!port ||
	   (data_rate != USB_VHCI_DATA_RATE_FULL &&
	    data_rate != USB_VHCI_DATA_RATE_LOW &&
	    data_rate != USB_VHCI_DATA_RATE_HIGH))
	{
		errno = EINVAL;
		return -1;
	}
	struct usb_vhci_ioc_port_stat ps;
	ps.status = USB_PORT_STAT_CONNECTION;
	if(data_rate == USB_VHCI_DATA_RATE_LOW)
		ps.status |= USB_PORT_STAT_LOW_SPEED;
	if(data_rate == USB_VHCI_DATA_RATE_HIGH)
		ps.status |= USB_PORT_STAT_HIGH_SPEED;
	ps.change = USB_PORT_STAT_C_CONNECTION;
	ps.index = port;
	ps.flags = 0;
	if(ioctl(fd, USB_VHCI_HCD_IOCPORTSTAT, &ps) == -1)
		return -1;
	return 0;
}

int usb_vhci_port_disconnect(int fd, uint8_t port)
{
	if(!port)
	{
		errno = EINVAL;
		return -1;
	}
	struct usb_vhci_ioc_port_stat ps;
	ps.status = 0;
	ps.change = USB_PORT_STAT_C_CONNECTION;
	ps.index = port;
	ps.flags = 0;
	if(ioctl(fd, USB_VHCI_HCD_IOCPORTSTAT, &ps) == -1)
		return -1;
	return 0;
}

int usb_vhci_port_disable(int fd, uint8_t port)
{
	if(!port)
	{
		errno = EINVAL;
		return -1;
	}
	struct usb_vhci_ioc_port_stat ps;
	ps.status = 0;
	ps.change = USB_PORT_STAT_C_ENABLE;
	ps.index = port;
	ps.flags = 0;
	if(ioctl(fd, USB_VHCI_HCD_IOCPORTSTAT, &ps) == -1)
		return -1;
	return 0;
}

int usb_vhci_port_resumed(int fd, uint8_t port)
{
	if(!port)
	{
		errno = EINVAL;
		return -1;
	}
	struct usb_vhci_ioc_port_stat ps;
	ps.status = 0;
	ps.change = USB_PORT_STAT_C_SUSPEND;
	ps.index = port;
	ps.flags = 0;
	if(ioctl(fd, USB_VHCI_HCD_IOCPORTSTAT, &ps) == -1)
		return -1;
	return 0;
}

int usb_vhci_port_overcurrent(int fd, uint8_t port, uint8_t set)
{
	if(!port)
	{
		errno = EINVAL;
		return -1;
	}
	struct usb_vhci_ioc_port_stat ps;
	ps.status = set ? USB_PORT_STAT_OVERCURRENT : 0;
	ps.change = USB_PORT_STAT_C_OVERCURRENT;
	ps.index = port;
	ps.flags = 0;
	if(ioctl(fd, USB_VHCI_HCD_IOCPORTSTAT, &ps) == -1)
		return -1;
	return 0;
}

int usb_vhci_port_reset_done(int fd, uint8_t port, uint8_t enable)
{
	if(!port)
	{
		errno = EINVAL;
		return -1;
	}
	struct usb_vhci_ioc_port_stat ps;
	ps.status = enable ? USB_PORT_STAT_ENABLE : 0;;
	ps.change = USB_PORT_STAT_C_RESET;
	ps.change |= enable ? 0 : USB_PORT_STAT_C_ENABLE;
	ps.index = port;
	ps.flags = 0;
	if(ioctl(fd, USB_VHCI_HCD_IOCPORTSTAT, &ps) == -1)
		return -1;
	return 0;
}

uint8_t usb_vhci_port_stat_triggers(const struct usb_vhci_port_stat *stat,
                                    const struct usb_vhci_port_stat *prev)
{
	uint8_t flags = 0;
	if(!(stat->status & USB_VHCI_PORT_STAT_ENABLE) &&
	    (prev->status & USB_VHCI_PORT_STAT_ENABLE))       flags |= USB_VHCI_PORT_STAT_TRIGGER_DISABLE;
	if( (stat->status & USB_VHCI_PORT_STAT_SUSPEND) &&
	   !(prev->status & USB_VHCI_PORT_STAT_SUSPEND))      flags |= USB_VHCI_PORT_STAT_TRIGGER_SUSPEND;
	if( (stat->flags & USB_VHCI_PORT_STAT_FLAG_RESUMING) &&
	   !(prev->flags & USB_VHCI_PORT_STAT_FLAG_RESUMING)) flags |= USB_VHCI_PORT_STAT_TRIGGER_RESUMING;
	if( (stat->status & USB_VHCI_PORT_STAT_RESET) &&
	   !(prev->status & USB_VHCI_PORT_STAT_RESET))        flags |= USB_VHCI_PORT_STAT_TRIGGER_RESET;
	if( (stat->status & USB_VHCI_PORT_STAT_POWER) &&
	   !(prev->status & USB_VHCI_PORT_STAT_POWER))        flags |= USB_VHCI_PORT_STAT_TRIGGER_POWER_ON;
	if(!(stat->status & USB_VHCI_PORT_STAT_POWER) &&
	    (prev->status & USB_VHCI_PORT_STAT_POWER))        flags |= USB_VHCI_PORT_STAT_TRIGGER_POWER_OFF;
	return flags;
}

int usb_vhci_to_errno(int32_t status, uint8_t iso_urb)
{
	switch(status)
	{
	case USB_VHCI_STATUS_SUCCESS:                return 0;
	case USB_VHCI_STATUS_PENDING:                return -EINPROGRESS;
	case USB_VHCI_STATUS_SHORT_PACKET:           return -EREMOTEIO;
	case USB_VHCI_STATUS_ERROR:                  return iso_urb ? -EXDEV : -EPROTO;
	case USB_VHCI_STATUS_CANCELED:               return -ECONNRESET; // or -ENOENT
	case USB_VHCI_STATUS_TIMEDOUT:               return -ETIMEDOUT;
	case USB_VHCI_STATUS_DEVICE_DISABLED:        return -ESHUTDOWN;
	case USB_VHCI_STATUS_DEVICE_DISCONNECTED:    return -ENODEV;
	case USB_VHCI_STATUS_BIT_STUFF:              return -EPROTO;
	case USB_VHCI_STATUS_CRC:                    return -EILSEQ;
	case USB_VHCI_STATUS_NO_RESPONSE:            return -ETIME;
	case USB_VHCI_STATUS_BABBLE:                 return -EOVERFLOW;
	case USB_VHCI_STATUS_STALL:                  return -EPIPE;
	case USB_VHCI_STATUS_BUFFER_OVERRUN:         return -ECOMM;
	case USB_VHCI_STATUS_BUFFER_UNDERRUN:        return -ENOSR;
	case USB_VHCI_STATUS_ALL_ISO_PACKETS_FAILED: return iso_urb ? -EINVAL : -EPROTO;
	default:                                     return -EPROTO;
	}
}

int32_t usb_vhci_from_errno(int errno, uint8_t iso_urb)
{
	switch(errno)
	{
	case 0:            return USB_VHCI_STATUS_SUCCESS;
	case -EINPROGRESS: return USB_VHCI_STATUS_PENDING;
	case -EREMOTEIO:   return USB_VHCI_STATUS_SHORT_PACKET;
	case -ENOENT:
	case -ECONNRESET:  return USB_VHCI_STATUS_CANCELED;
	case -ETIMEDOUT:   return USB_VHCI_STATUS_TIMEDOUT;
	case -ESHUTDOWN:   return USB_VHCI_STATUS_DEVICE_DISABLED;
	case -ENODEV:      return USB_VHCI_STATUS_DEVICE_DISCONNECTED;
	case -EPROTO:      return USB_VHCI_STATUS_BIT_STUFF;
	case -EILSEQ:      return USB_VHCI_STATUS_CRC;
	case -ETIME:       return USB_VHCI_STATUS_NO_RESPONSE;
	case -EOVERFLOW:   return USB_VHCI_STATUS_BABBLE;
	case -EPIPE:       return USB_VHCI_STATUS_STALL;
	case -ECOMM:       return USB_VHCI_STATUS_BUFFER_OVERRUN;
	case -ENOSR:       return USB_VHCI_STATUS_BUFFER_UNDERRUN;
	case -EINVAL:      return iso_urb ? USB_VHCI_STATUS_ALL_ISO_PACKETS_FAILED :
	                                    USB_VHCI_STATUS_ERROR;
	default:           return USB_VHCI_STATUS_ERROR;
	}
}

int usb_vhci_to_iso_packets_errno(int32_t status)
{
	return usb_vhci_to_errno(status, 0);
}

int32_t usb_vhci_from_iso_packets_errno(int errno)
{
	return usb_vhci_from_errno(errno, 0);
}

