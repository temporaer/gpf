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
#include "OneMethodOnePointerParamInterface.hpp"
#include <algorithm>
#include <glog/logging.h>

namespace zmq_reactor
{

	typedef OneMthdOneParamPtrInterface<zmq::socket_t> PollEventInterface;


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

	template <class T, class V, class R>
		inline
		void trait(T& cb, V* v, R* r)
		{
			cb(*v, r); 
		};
	template <class T, class V>
		inline
		void trait(T& cb, V* v, void* r)
		{
			cb(*v); 
		};


	template <class SOCKET = zmq::socket_t>
		struct reactor
		{
			typedef boost::function<void (zmq::socket_t&)>  callback_t;

			bool add(SOCKET& v, short event, callback_t cb, bool checkIfSocketAddedTwice = true)
			{
				zmq_pollitem_t item = {0,0,0,0};
				set(v, item);
				item.events = event;
				return addImpl(item, v, cb, checkIfSocketAddedTwice);
			}
			bool add(short event, callback_t cb){
				if(event == ZMQ_POLLIN)
					m_pre_receive_callbacks.push_back(cb);
				return true;
			}
			bool remove(SOCKET& v)
			{
				return removeImpl(v);
			}
			template <typename STATE>
			int operator()(STATE* state = 0, int timeout = -1)
			{
				int ret = zmq::poll (&items_[0], items_.size(), timeout);
				if ((ret == 0) || (ret == -1) )
					return ret;

				typename std::vector<SOCKET*>::iterator skit = socks_.begin();
				std::vector<callback_t>::iterator cit        = m_callbacks.begin(); // outside "for" to keep VS2010 happy
				std::vector<zmq_pollitem_t>::iterator it     = items_.begin();      // keep clang happy
				for (; it != items_.end(); ++it, ++cit, ++skit)
				{
					if (it->revents & it->events)
					{	
						callback_t& cb = *cit;
						SOCKET* v    = *skit;

						BOOST_FOREACH(callback_t& cb, m_pre_receive_callbacks){
							trait(cb,v,state);
						}
						trait(cb,v, state);
					}
				}
				return ret;
			}
			int operator()( int timeout = -1) //  overload to keep VS2010 happy - no defaults for functions
			{
				return this->operator()((void*)0, timeout);
			}

			template <typename STATE>
				int run(	STATE* state = 0,  int timeout = -1, 
						int (*begin)(STATE*, reactor&, int&) = 0, 
						int (*end)(STATE*, reactor&, int&, int) = 0)
				{
					while (1)
					{
						int tmout = timeout;
						if (begin)
						{
							int ret = begin(state, *this, tmout);
							if (ret) return ret;
						}
						int ret = this->operator()(state, tmout);
						if (ret == -1)
							return ret;
						if (end)
						{
							int ret = end(state, *this, tmout, ret);
							if (ret) return ret;
						}
					}
				}
			int run(int timeout = -1) //  overload to keep VS2010 happy - no defaults for functions
			{
				return run((void*)0, timeout, 0, 0);
			}

			private:
			template <class K, class TT>
				static int getIndex(const K& k, TT* t)
				{
					typename std::vector<TT*>::const_iterator it = std::find( k.begin(), k.end(), t );
					return it == k.end() ? -1 : it - k.begin();
				}
			bool addImpl(zmq_pollitem_t& item, SOCKET& v, callback_t cb, bool checkIfSocketAddedTwice)
			{
				//if added twice it hangs
				if (checkIfSocketAddedTwice && (getIndex(socks_, &v) > -1) )
					return false;
				items_    .push_back(item);
				m_callbacks.push_back(cb);
				socks_    .push_back(&v);
				return true;
			}

			template <class K>
				static void removeImpl( K& k, size_t pos)
				{
					k.erase(k.begin() + pos);
				}

			bool removeImpl(SOCKET& v)
			{
				int pos = getIndex(socks_, &v);
				if (pos == -1)
					return false;
				removeImpl(items_, pos);
				removeImpl(m_callbacks, pos);
				removeImpl(socks_, pos);
				return true;
			}
			private:
			std::vector<zmq_pollitem_t> items_;
			std::vector<callback_t>       m_callbacks;

			// TODO: add timeouts callbacks
			std::vector<callback_t>       m_pre_receive_callbacks;
			std::vector<SOCKET*>        socks_;
		};

}


#endif

