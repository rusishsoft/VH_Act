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

#include <stdlib.h>
#include <unistd.h>
#include <new>
#include "libusb_vhci.h"

namespace usb
{
	namespace vhci
	{
		local_hcd::local_hcd(uint8_t ports) throw(std::exception) :
			hcd(ports),
			fd(-1),
			id(),
			usb_bus_num(),
			bus_id(),
			port_info(NULL)
		{
			uint8_t c = get_port_count();
			char* _bus_id(NULL);
			fd = usb_vhci_open(c, &id, &usb_bus_num, &_bus_id);
			if(fd == -1) throw std::exception();
			if(_bus_id)
			{
				bus_id.assign(_bus_id);
				free(_bus_id);
			}
			if(c) port_info = new _port_info[c];
			init_bg_thread();
		}

		local_hcd::~local_hcd() throw()
		{
			join_bg_thread();
			usb_vhci_close(fd);
			delete[] port_info;
		}

		// caller has _lock
		uint8_t local_hcd::address_from_port(uint8_t port) const throw(std::invalid_argument, std::out_of_range)
		{
			if(!port) throw std::invalid_argument("port");
			if(port > get_port_count()) throw std::out_of_range("port");
			return port_info[port - 1].adr;
		}

		// caller has _lock
		uint8_t local_hcd::port_from_address(uint8_t address) const throw(std::invalid_argument)
		{
			if(address > 0x7f) throw std::invalid_argument("address");
			for(uint8_t i(0); i < get_port_count(); i++)
				if(port_info[i].adr == address)
					return i + 1;
			return 0;
		}

		void local_hcd::bg_work() volatile throw()
		{
			local_hcd& _this(const_cast<local_hcd&>(*this));
			usb_vhci_work w;
			int res(usb_vhci_fetch_work(_this.fd, &w));
			if(res == -1)
			{
				if(errno == ETIMEDOUT || errno == EINTR || errno == ENODATA)
					return;
				// TODO: debug msg
				return;
			}
			uint8_t index;
			switch(w.type)
			{
			case USB_VHCI_WORK_TYPE_PORT_STAT:
			{
				index = w.work.port_stat.index;
				port_stat nps(w.work.port_stat.status,
				              w.work.port_stat.change,
				              w.work.port_stat.flags);
				// TODO: debug msg
				if(!index || index > _this.get_port_count())
					break;
				bool nomem_retry(false);
				port_stat_work* psw(NULL);
			retry_ps:
				if(nomem_retry)
				{
					usleep(100000);
					if(is_thread_shutdown())
					{
						delete psw;
						return;
					}
				}
				else
				{
					nomem_retry = true;
				}
				lock _(get_lock()); //  vvvv LOCKED vvvv  --  ^^^^ NOT LOCKED ^^^^
				try
				{
					if(!psw) psw = new port_stat_work(index, nps, _this.port_info[index - 1].stat);
					else
					{
						// reuse already allocated mem
						psw->~port_stat_work();
						new(psw) port_stat_work(index, nps, _this.port_info[index - 1].stat);
					}
					_this.enqueue_work(psw);
				}
				catch(std::bad_alloc)
				{
					// jump outside the lock and wait for others to free mem
					goto retry_ps;
				}
				_this.port_info[index - 1].stat = nps;
				if(nps.get_connection_changed())
				{
					// invalidate address on CONNECTION state change
					_this.port_info[index - 1].adr = 0xff;
				}
				if(nps.get_reset_changed() && !nps.get_reset() && nps.get_enable())
				{
					// set address to 0 after successfull RESET
					_this.port_info[index - 1].adr = 0x00;
				}
				// TODO: do we need to check for any other state changes here?
				_this.on_work_enqueued();
				break;
			} //  vvvv NOT LOCKED vvvv  --  ^^^^ LOCKED ^^^^
			case USB_VHCI_WORK_TYPE_PROCESS_URB:
			{
				bool nomem_retry(false);
				process_urb_work* puw(NULL);
			retry_pu:
				if(nomem_retry)
				{
					usleep(100000);
					if(is_thread_shutdown())
					{
						delete puw; // dtor of puw deletes the urb and the data buffers, too
						return;
					}
				}
				else
				{
					nomem_retry = true;
				}
				if(w.work.urb.buffer_length)
				{
					do
					{
						w.work.urb.buffer = new(std::nothrow) uint8_t[w.work.urb.buffer_length];
						if(!w.work.urb.buffer)
						{
							// wait for others to free mem
							usleep(100000);
							if(is_thread_shutdown()) return;
						}
					} while(!w.work.urb.buffer);
				}
				if(w.work.urb.packet_count)
				{
					do
					{
						w.work.urb.iso_packets = new(std::nothrow) usb_vhci_iso_packet[w.work.urb.packet_count];
						if(!w.work.urb.iso_packets)
						{
							// wait for others to free mem
							usleep(100000);
							if(is_thread_shutdown()) return;
						}
					} while(!w.work.urb.iso_packets);
				}
				usb::urb* u(NULL);
				while(!u)
				{
					if(!(u = new(std::nothrow) usb::urb(w.work.urb, true)))
					{
						// wait for others to free mem
						usleep(100000);
						if(is_thread_shutdown()) return;
					}
				}
				if(res)
				{
					res = usb_vhci_fetch_data(fd, u->get_internal());
					if(res == -1)
					{
						delete u;
						// TODO: debug msg
						//if(errno == ECANCELED) {} else {}
						break;
					}
				}
				lock _(get_lock()); //  vvvv LOCKED vvvv  --  ^^^^ NOT LOCKED ^^^^
				index = _this.port_from_address(w.work.urb.devadr);
				// TODO: debug msg
				if(!index)
				{
					delete puw; // dtor of puw deletes the urb and the data buffers, too
					break;
				}
				if(!puw)
				{
					if(!(puw = new(std::nothrow) process_urb_work(index, u)))
					{
						// jump outside the lock and wait for others to free mem
						goto retry_pu;
					}
				}
				else
				{
					// reuse already allocated mem
					puw->~process_urb_work();
					new(puw) process_urb_work(index, u);
				}
				uint8_t rollback_address(_this.port_info[index - 1].adr);
				if(u->is_control())
				{
					// SET_ADDRESS?
					if(!u->get_endpoint_number() &&
					   !u->get_bmRequestType() &&
					   u->get_bRequest() == 5)
					{
						uint16_t val(u->get_wValue());
						if(val > 0x7f)
							u->stall();
						else
						{
							u->ack();
							_this.port_info[index - 1].adr = static_cast<uint8_t>(val);
						}
					}
				}
				try
				{
					_this.enqueue_work(puw);
				}
				catch(std::bad_alloc)
				{
					// rollback changes on 'this'
					_this.port_info[index - 1].adr = rollback_address;
					// jump outside the lock and wait for others to free mem
					goto retry_pu;
				}
				_this.on_work_enqueued();
				break;
			} //  vvvv NOT LOCKED vvvv  --  ^^^^ LOCKED ^^^^
			case USB_VHCI_WORK_TYPE_CANCEL_URB:
				cancel_process_urb_work(w.work.handle);
				break;
			}
		}

		// caller has _lock
		void local_hcd::canceling_work(work* w, bool in_progress) throw(std::exception)
		{
			process_urb_work* uw;
			if(in_progress && (uw = dynamic_cast<process_urb_work*>(w)))
			{
				cancel_urb_work* cw = new cancel_urb_work(uw->get_port(), uw->get_urb()->get_handle());
				try { enqueue_work(cw); }
				catch(...)
				{
					delete cw;
					throw;
				}
				on_work_enqueued();
			}
		}

		// caller has _lock
		void local_hcd::finishing_work(work* w) throw(std::exception)
		{
			process_urb_work* uw(dynamic_cast<process_urb_work*>(w));
			if(uw)
			{
				const usb::urb* urb(uw->get_urb());
				if(usb_vhci_giveback(fd, urb->get_internal()) == -1)
				{
					// TODO: debug msg
				}
			}
		}

		const port_stat& local_hcd::get_port_stat(uint8_t port) volatile throw(std::invalid_argument, std::out_of_range)
		{
			if(!port) throw std::invalid_argument("port");
			if(port > get_port_count()) throw std::out_of_range("port");
			lock _(get_lock());
			return port_info[port - 1].stat;
		}

		void local_hcd::port_connect(uint8_t port, usb::data_rate rate) volatile throw(std::exception)
		{
			if(!port) throw std::invalid_argument("port");
			if(port > get_port_count()) throw std::out_of_range("port");
			if(usb_vhci_port_connect(fd, port, rate) == -1)
				throw std::exception();
		}

		void local_hcd::port_disconnect(uint8_t port) volatile throw(std::exception)
		{
			if(!port) throw std::invalid_argument("port");
			if(port > get_port_count()) throw std::out_of_range("port");
			if(usb_vhci_port_disconnect(fd, port) == -1)
				throw std::exception();
		}

		void local_hcd::port_disable(uint8_t port) volatile throw(std::exception)
		{
			if(!port) throw std::invalid_argument("port");
			if(port > get_port_count()) throw std::out_of_range("port");
			if(usb_vhci_port_disable(fd, port) == -1)
				throw std::exception();
		}

		void local_hcd::port_resumed(uint8_t port) volatile throw(std::exception)
		{
			if(!port) throw std::invalid_argument("port");
			if(port > get_port_count()) throw std::out_of_range("port");
			if(usb_vhci_port_resumed(fd, port) == -1)
				throw std::exception();
		}

		void local_hcd::port_overcurrent(uint8_t port, bool set) volatile throw(std::exception)
		{
			if(!port) throw std::invalid_argument("port");
			if(port > get_port_count()) throw std::out_of_range("port");
			if(usb_vhci_port_overcurrent(fd, port, set) == -1)
				throw std::exception();
		}

		void local_hcd::port_reset_done(uint8_t port, bool enable) volatile throw(std::exception)
		{
			if(!port) throw std::invalid_argument("port");
			if(port > get_port_count()) throw std::out_of_range("port");
			if(usb_vhci_port_reset_done(fd, port, enable) == -1)
				throw std::exception();
		}
	}
}
