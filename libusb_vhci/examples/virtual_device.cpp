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
 * 3. Run "./virtual_device" in the examples subdirectory. You have to
 *    be root, if you did not make /dev/usb-vhci accessible for all
 *    users (chmod 666 /dev/usb-vhci).
 *
 * Now type "dmesg" in another shell or "cat /proc/bus/usb/devices" if
 * you have usbfs mounted. You should see a dummy usb device, which does
 * nothing else than answering a few control requests from the usb core.
 */

#include <pthread.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include "../src/libusb_vhci.h"

bool has_work(true), waiting_for_work(false);
pthread_mutex_t has_work_mutex;
pthread_cond_t has_work_cv;

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

const uint8_t* str1_desc =
	reinterpret_cast<const uint8_t*>("\x1a\x03H\0e\0l\0l\0o\0 \0W\0o\0r\0l\0d\0!");

void signal_work_enqueued(void* arg, usb::vhci::hcd& from) throw()
{
	pthread_mutex_lock(&has_work_mutex);
	has_work = true;
	if(waiting_for_work)
	{
		waiting_for_work = false;
		pthread_cond_signal(&has_work_cv);
	}
	pthread_mutex_unlock(&has_work_mutex);
}

void process_urb(usb::urb* urb)
{
	if(!urb->is_control())
	{
		std::cout << "not CONTROL" << std::endl;
		return;
	}
	if(urb->get_endpoint_number())
	{
		std::cout << "not ep0" << std::endl;
		urb->stall();
		return;
	}

	uint8_t rt(urb->get_bmRequestType());
	uint8_t r(urb->get_bRequest());
	if(rt == 0x00 && r == URB_RQ_SET_ADDRESS)
	{
		std::cout << "SET_ADDRESS" << std::endl;
		urb->ack();
	}
	else if(rt == 0x00 && r == URB_RQ_SET_CONFIGURATION)
	{
		std::cout << "SET_CONFIGURATION" << std::endl;
		urb->ack();
	}
	else if(rt == 0x00 && r == URB_RQ_SET_INTERFACE)
	{
		std::cout << "SET_INTERFACE" << std::endl;
		urb->ack();
	}
	else if(rt == 0x80 && r == URB_RQ_GET_DESCRIPTOR)
	{
		std::cout << "GET_DESCRIPTOR" << std::endl;
		int l(urb->get_wLength());
		uint8_t* buffer(urb->get_buffer());
		switch(urb->get_wValue() >> 8)
		{
		case 1:
			std::cout << "DEVICE_DESCRIPTOR" << std::endl;
			if(dev_desc[0] < l) l = dev_desc[0];
			std::copy(dev_desc, dev_desc + l, buffer);
			urb->set_buffer_actual(l);
			urb->ack();
			break;
		case 2:
			std::cout << "CONFIGURATION_DESCRIPTOR" << std::endl;
			if(conf_desc[2] < l) l = conf_desc[2];
			std::copy(conf_desc, conf_desc + l, buffer);
			urb->set_buffer_actual(l);
			urb->ack();
			break;
		case 3:
			std::cout << "STRING_DESCRIPTOR" << std::endl;
			switch(urb->get_wValue() & 0xff)
			{
			case 0:
				if(str0_desc[0] < l) l = str0_desc[0];
				std::copy(str0_desc, str0_desc + l, buffer);
				urb->set_buffer_actual(l);
				urb->ack();
				break;
			case 1:
				if(str1_desc[0] < l) l = str1_desc[0];
				std::copy(str1_desc, str1_desc + l, buffer);
				urb->set_buffer_actual(l);
				urb->ack();
				break;
			default:
				urb->stall();
				break;
			}
		default:
			urb->stall();
			break;
		}
	}
	else
		urb->stall();
}

int main()
{
	pthread_mutex_init(&has_work_mutex, NULL);
	pthread_cond_init(&has_work_cv, NULL);

	usb::vhci::local_hcd hcd(1);
	std::cout << "created " << hcd.get_bus_id() << " (bus# " << hcd.get_usb_bus_num() << ")" << std::endl;
	hcd.add_work_enqueued_callback(usb::vhci::hcd::callback(&signal_work_enqueued, NULL));

	bool cont(false);
	while(true)
	{
		if(!cont)
		{
			pthread_mutex_lock(&has_work_mutex);
			if(!has_work)
			{
				waiting_for_work = true;
				pthread_cond_wait(&has_work_cv, &has_work_mutex);
			}
			else has_work = false;
			pthread_mutex_unlock(&has_work_mutex);
		}
		usb::vhci::work* work;
		cont = hcd.next_work(&work);
		if(work)
		{
			if(usb::vhci::port_stat_work* psw = dynamic_cast<usb::vhci::port_stat_work*>(work))
			{
				std::cout << "got port stat work" << std::endl;
				std::cout << "status: 0x" << std::setw(4) << std::setfill('0') <<
				             std::right << std::hex << psw->get_port_stat().get_status() << std::endl;
				std::cout << "change: 0x" << std::setw(4) << std::setfill('0') <<
				             std::right << psw->get_port_stat().get_change() << std::endl;
				std::cout << "flags:  0x" << std::setw(2) << std::setfill('0') <<
				             std::right << static_cast<int>(psw->get_port_stat().get_flags()) << std::endl;
				if(psw->get_port() != 1)
				{
					std::cerr << "invalid port" << std::endl;
					return 1;
				}
				if(psw->triggers_power_off())
				{
					std::cout << "port is powered off" << std::endl;
				}
				if(psw->triggers_power_on())
				{
					std::cout << "port is powered on -> connecting device" << std::endl;
					hcd.port_connect(1, usb::data_rate_full);
				}
				if(psw->triggers_reset())
				{
					std::cout << "port is resetting" << std::endl;
					if(hcd.get_port_stat(1).get_connection())
					{
						std::cout << "-> completing reset" << std::endl;
						hcd.port_reset_done(1);
					}
				}
				if(psw->triggers_resuming())
				{
					std::cout << "port is resuming" << std::endl;
					if(hcd.get_port_stat(1).get_connection())
					{
						std::cout << "-> completing resume" << std::endl;
						hcd.port_resumed(1);
					}
				}
				if(psw->triggers_suspend())
					std::cout << "port is suspended" << std::endl;
				if(psw->triggers_disable())
					std::cout << "port is disabled" << std::endl;
			}
			else if(usb::vhci::process_urb_work* puw = dynamic_cast<usb::vhci::process_urb_work*>(work))
			{
				std::cout << "got process urb work" << std::endl;
				process_urb(puw->get_urb());
			}
			else if(usb::vhci::cancel_urb_work* cuw = dynamic_cast<usb::vhci::cancel_urb_work*>(work))
			{
				std::cout << "got cancel urb work" << std::endl;
			}
			else
			{
				std::cerr << "got invalid work" << std::endl;
				return 1;
			}
			hcd.finish_work(work);
		}
	}

	pthread_mutex_destroy(&has_work_mutex);
	pthread_cond_destroy(&has_work_cv);
	return 0;
}

