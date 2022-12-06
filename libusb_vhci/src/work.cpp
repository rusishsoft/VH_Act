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

#include "libusb_vhci.h"

namespace usb
{
	namespace vhci
	{
		work::work(uint8_t port) throw(std::invalid_argument) : port(port), canceled(false)
		{
			if(port == 0) throw std::invalid_argument("port");
		}

		work::~work() throw()
		{
		}

		void work::cancel() throw()
		{
			canceled = true;
		}

		process_urb_work::process_urb_work(uint8_t port, usb::urb* urb) throw(std::invalid_argument) :
			work(port),
			urb(urb)
		{
			if(!urb) throw std::invalid_argument("urb");
		}

		process_urb_work::process_urb_work(const process_urb_work& work) throw(std::bad_alloc) :
			usb::vhci::work(work),
			urb(new usb::urb(*work.urb))
		{
		}

		process_urb_work& process_urb_work::operator=(const process_urb_work& work) throw(std::bad_alloc)
		{
			usb::vhci::work::operator=(work);
			delete urb;
			urb = new usb::urb(*work.urb);
			return *this;
		}

		process_urb_work::~process_urb_work() throw()
		{
			delete urb;
		}

		cancel_urb_work::cancel_urb_work(uint8_t port, uint64_t handle) throw(std::invalid_argument) :
			work(port),
			handle(handle)
		{
		}

		port_stat_work::port_stat_work(uint8_t port, const port_stat& stat) throw(std::invalid_argument) :
			work(port),
			stat(stat),
			trigger_flags(0)
		{
		}

		port_stat_work::port_stat_work(uint8_t port,
		                               const port_stat& stat,
		                               const port_stat& prev) throw(std::invalid_argument) :
			work(port),
			stat(stat),
			trigger_flags(0)
		{
			if(!stat.get_enable() && prev.get_enable())     trigger_flags |= USB_VHCI_PORT_STAT_TRIGGER_DISABLE;
			if(stat.get_suspend() && !prev.get_suspend())   trigger_flags |= USB_VHCI_PORT_STAT_TRIGGER_SUSPEND;
			if(stat.get_resuming() && !prev.get_resuming()) trigger_flags |= USB_VHCI_PORT_STAT_TRIGGER_RESUMING;
			if(stat.get_reset() && !prev.get_reset())       trigger_flags |= USB_VHCI_PORT_STAT_TRIGGER_RESET;
			if(stat.get_power() && !prev.get_power())       trigger_flags |= USB_VHCI_PORT_STAT_TRIGGER_POWER_ON;
			else if(!stat.get_power() && prev.get_power())  trigger_flags |= USB_VHCI_PORT_STAT_TRIGGER_POWER_OFF;
		}
	}
}
