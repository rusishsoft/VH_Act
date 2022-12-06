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
		hcd::hcd(uint8_t ports) throw(std::invalid_argument, std::bad_alloc) :
			work_enqueued_callbacks(),
			bg_thread(),
			thread_shutdown(false),
			thread_sync(),
			port_count(ports),
			_lock(),
			inbox(),
			processing()
		{
			if(ports == 0) throw std::invalid_argument("ports");
			pthread_mutex_init(&thread_sync, NULL);
			pthread_mutex_init(&_lock, NULL);
		}

		hcd::~hcd() throw()
		{
			join_bg_thread();
			for(std::deque<work*>::iterator w(inbox.begin()); w < inbox.end(); w++)
				delete *w;
			for(std::list<work*>::iterator w(processing.begin()); w != processing.end(); w++)
				delete *w;
			pthread_mutex_destroy(&_lock);
			pthread_mutex_destroy(&thread_sync);
		}

		// caller has _lock
		void hcd::on_work_enqueued() throw()
		{
			for(std::vector<callback>::const_iterator i(work_enqueued_callbacks.begin());
			    i < work_enqueued_callbacks.end();
			    i++)
			{
				i->call(*this);
			}
		}

		// caller has _lock
		void hcd::enqueue_work(work* w) throw(std::bad_alloc)
		{
			inbox.push_back(w);
		}

		void hcd::init_bg_thread() volatile throw(std::exception)
		{
			pthread_attr_t attr;
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
			// TODO: set priority
			int res;
			pthread_t t;
			{
				lock _(thread_sync);
				if(bg_thread != pthread_t())
					throw std::exception();
				res = pthread_create(&t, NULL, bg_thread_start, const_cast<hcd*>(this));
				if(res) goto cleanup;
				bg_thread = t;
			}
		cleanup:
			pthread_attr_destroy(&attr);
			if(res) throw std::exception();
		}

		void hcd::join_bg_thread() volatile throw()
		{
			lock _(thread_sync);
			if(bg_thread == pthread_t()) return;
			thread_shutdown = true;
			pthread_join(bg_thread, NULL);
			thread_shutdown = false;
			bg_thread = pthread_t();
		}

		void* hcd::bg_thread_start(void* _this) throw()
		{
			hcd& dev = *reinterpret_cast<hcd*>(_this);
			while(!dev.thread_shutdown)
				dev.bg_work();
			return NULL;
		}

		bool hcd::next_work(work** w) volatile throw(std::bad_alloc)
		{
			*w = NULL;
			lock _(_lock);
			hcd& _this(const_cast<hcd&>(*this));
			std::deque<work*>::size_type len(_this.inbox.size());
			while(len)
			{
				work* _w(_this.inbox.front());
				_this.inbox.pop_front();
				if(!_w->is_canceled())
				{
					_this.processing.push_back(_w);
					*w = _w;
					return len != 1;
				}
				delete _w;
				len = _this.inbox.size();
			}
			return false;
		}

		void hcd::finish_work(work* w) volatile throw(std::exception)
		{
			{
				lock _(_lock);
				hcd& _this(const_cast<hcd&>(*this));
				_this.finishing_work(w);
				_this.processing.remove(w);
			}
			delete w;
		}

		bool hcd::cancel_process_urb_work(uint64_t handle) volatile throw(std::exception)
		{
			lock _(_lock);
			hcd& _this(const_cast<hcd&>(*this));
			process_urb_work* wrk(NULL);
			for(std::deque<work*>::iterator w(_this.inbox.begin()); w < _this.inbox.end(); w++)
			{
				process_urb_work* uw = dynamic_cast<process_urb_work*>(*w);
				if(uw)
				{
					if(uw->get_urb()->get_handle() == handle)
					{
						wrk = uw;
						break;
					}
				}
			}
			if(!wrk)
			{
				for(std::list<work*>::iterator w(_this.processing.begin()); w != _this.processing.end(); w++)
				{
					process_urb_work* uw = dynamic_cast<process_urb_work*>(*w);
					if(uw)
					{
						if(uw->get_urb()->get_handle() == handle)
						{
							wrk = uw;
							break;
						}
					}
				}
				if(wrk)
				{
					_this.canceling_work(wrk, true);
					return true;
				}
			}
			else
			{
				wrk->cancel();
				_this.canceling_work(wrk, false);
				_this.finishing_work(wrk);
			}
			return false;
		}

		// caller has _lock
		void hcd::canceling_work(work* w, bool in_progress) throw(std::exception) { }
		// caller has _lock
		void hcd::finishing_work(work* w) throw(std::exception) { }

		void hcd::add_work_enqueued_callback(callback c) volatile throw(std::bad_alloc)
		{
			lock _(_lock);
			hcd& _this(const_cast<hcd&>(*this));
			_this.work_enqueued_callbacks.push_back(c);
		}

		void hcd::remove_work_enqueued_callback(callback c) volatile throw()
		{
			lock _(_lock);
			hcd& _this(const_cast<hcd&>(*this));
			std::vector<callback>& wec(_this.work_enqueued_callbacks);
			for(std::vector<callback>::iterator i(wec.begin()); i < wec.end(); i++)
			{
				if(*i == c)
				{
					wec.erase(i);
					break;
				}
			}
		}
	}
}
