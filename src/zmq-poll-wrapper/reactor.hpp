/*
 * ----------------------------------------------------------
 *
 * Copyright 2010 Radu Braniste
 *
 * ----------------------------------------------------------
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 */


#ifndef reactor_hpp
#define reactor_hpp


#include <zmq.hpp>
#include <zmq_utils.h> 
#include <vector>
#include <functional>
#include <boost/foreach.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <algorithm>
#include <glog/logging.h>
#include "timer.hpp"

namespace zmq_reactor
{

	template <class T>
		inline
		void set (T& t, zmq_pollitem_t& item)
		{
			item.fd = t;
		}
	template <>
		inline
		void set<zmq::socket_t> (zmq::socket_t& v, zmq_pollitem_t& item)
		{
			item.socket = v;
		}


		struct reactor
		{
			typedef zmq::socket_t   socket_t;
			typedef boost::shared_ptr<socket_t>   socket_ptr;
			typedef boost::function<void (socket_t&)>        socket_activity_callback_t;
			typedef boost::function<void (reactor*)>         timeout_callback_t;
			zmq::context_t& m_ctx;

			socket_ptr m_pub_internal_event;
			socket_ptr m_sub_internal_event;

			reactor(zmq::context_t& ctx)
			:m_ctx(ctx),
			 m_pub_internal_event(new socket_t(ctx, ZMQ_PUB)),
			 m_sub_internal_event(new socket_t(ctx, ZMQ_SUB))
			{
				m_pub_internal_event->bind("inproc://reactor");
				m_sub_internal_event->connect("inproc://reactor");
				m_sub_internal_event->setsockopt(ZMQ_SUBSCRIBE, "", 0);
				add(m_sub_internal_event, ZMQ_POLLIN, boost::bind(&reactor::_shutdown,this,_1));
			}

			void add(const gpf::deadline_timer& dt){
			       	m_timer_queue.queue.push(boost::shared_ptr<gpf::deadline_timer>(new gpf::deadline_timer(dt)));
			}
			void add(boost::shared_ptr<gpf::deadline_timer> dt){
			       	m_timer_queue.queue.push(dt);
			}
			bool add(socket_ptr v, short event, socket_activity_callback_t cb, bool checkIfSocketAddedTwice = true)
			{
				zmq_pollitem_t item = {0,0,0,0};
				set(*v, item);
				item.events = event;
				return addImpl(item, v, cb, checkIfSocketAddedTwice);
			}
			bool add(short event, socket_activity_callback_t cb){
				if(event == ZMQ_POLLIN)
					m_pre_receive_callbacks.push_back(cb);
				return true;
			}
			bool remove(socket_ptr v)
			{
				return removeImpl(v);
			}
			void shutdown(std::string s=""){ 
				zmq::message_t msg(s.size());
				strncpy((char*)msg.data(), s.c_str(), s.size());
				m_pub_internal_event->send(msg);
			}
			void run(long timeout = -1){
				m_stop_requested = false;
				int ret;
				while(!m_stop_requested && (ret=operator()(timeout))>=0);
				LOG(INFO)<<"Hub stopped, req: "<<m_stop_requested << " ret: "<<ret;
			}
			int operator()(long timeout = -1)
			{
				if(timeout < 0)
					timeout = m_timer_queue.next_timeout();
				else
					timeout = std::min(timeout, m_timer_queue.next_timeout());
				int ret = zmq::poll (&items_[0], items_.size(), timeout);
				if( ret==0 ){
					// timeout
					m_timer_queue.fire_due(this);
					return ret;
				}else if(ret < 0){
					// error
					throw std::runtime_error("zmq::poll returned an error");
				}
				// socket fired

				std::vector<socket_ptr>::iterator skit = socks_.begin();
				std::vector<socket_activity_callback_t>::iterator cit        = m_callbacks.begin(); // outside "for" to keep VS2010 happy
				std::vector<zmq_pollitem_t>::iterator it     = items_.begin();      // keep clang happy
				for (; it != items_.end(); ++it, ++cit, ++skit)
				{
					if (it->revents & it->events)
					{	
						socket_activity_callback_t& cb = *cit;
						socket_ptr& v    = *skit;

						BOOST_FOREACH(socket_activity_callback_t& cb, m_pre_receive_callbacks){
							cb(*v);
						}
						cb(*v);
					}
				}
				return ret;
			}

			private:
			void _shutdown(zmq::socket_t& sock){
				zmq::message_t msg;
			       	sock.recv(&msg);
				std::string reason((char*)msg.data(),msg.size());
				LOG(INFO)<<"Loop shutdown requested via inproc call: "<<reason;
				m_stop_requested = true;
			}
			template <class K, class TT>
				static int getIndex(const K& k, boost::shared_ptr<TT>& t)
				{
					typename std::vector<boost::shared_ptr<TT> >::const_iterator it = std::find( k.begin(), k.end(), t );
					return it == k.end() ? -1 : it - k.begin();
				}

			bool addImpl(zmq_pollitem_t& item, socket_ptr v, socket_activity_callback_t cb, bool checkIfSocketAddedTwice)
			{
				//if added twice it hangs
				if (checkIfSocketAddedTwice && (getIndex(socks_, v) > -1) )
					return false;
				items_    .push_back(item);
				m_callbacks.push_back(cb);
				socks_    .push_back(v);
				return true;
			}

			template <class K>
				static void removeImpl( K& k, size_t pos)
				{
					k.erase(k.begin() + pos);
				}

			bool removeImpl(socket_ptr v)
			{
				int pos = getIndex(socks_, v);
				if (pos == -1)
					return false;
				removeImpl(items_, pos);
				removeImpl(m_callbacks, pos);
				removeImpl(socks_, pos);
				return true;
			}
			private:
			std::vector<zmq_pollitem_t> items_;
			std::vector<socket_activity_callback_t>       m_callbacks;

			bool                                          m_stop_requested;
			gpf::timer_queue                              m_timer_queue;

			std::vector<socket_activity_callback_t>       m_pre_receive_callbacks;
			std::vector<socket_ptr>        socks_;
		};

}


#endif

