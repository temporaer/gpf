#ifndef __GPF_TIMER_HPP__
#     define __GPF_TIMER_HPP__

#include <queue>
#include <ctime>
#include <limits>
#include <glog/logging.h>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace zmq_reactor
{
	struct reactor;
}
namespace gpf
{
	struct deadline_timer{
		typedef boost::function<void (zmq_reactor::reactor*)> callback_t;
		boost::posix_time::ptime m_deadline;
		callback_t               m_callback;
		bool                     m_active;

		/**
		 * construct a deadline_timer using a deadline
		 */
		deadline_timer(const boost::posix_time::ptime& deadline, callback_t cb)
			: m_deadline(deadline)
			, m_callback(cb)
			, m_active(true)
		{ 
		}

		/**
		 * construct a deadline_timer using a time duration
		 */
		deadline_timer(const boost::posix_time::time_duration& waiting_time, callback_t cb)
			: m_deadline(boost::posix_time::microsec_clock::universal_time()+waiting_time)
			, m_callback(cb)
			, m_active(true)
		{ 
		}

		inline void set_inactive(bool b=true){ m_active = !b; }

		/**
		 * return how long we have to wait until this event fires
		 */
		inline
		boost::posix_time::time_duration
	       	get_timeout()const{
			boost::posix_time::time_duration td = m_deadline - boost::posix_time::microsec_clock::universal_time();
			if(td.is_negative()){
				return m_deadline-m_deadline; // 0-time
			}
			return td;
		}

		/**
		 * test if time has come
		 */
		inline bool is_due(const boost::posix_time::ptime& now)const{
			return now>=m_deadline;
		}

		/**
		 * fire event
		 */
		inline void fire(zmq_reactor::reactor* r)const{
			if(m_active)
				m_callback(r);
		}

		/**
		 * determine which timer is closest to its deadline
		 */
		inline 
		bool operator<(const deadline_timer& o)const{
			return m_deadline > o.m_deadline;
		}
	};

	/// stores multiple timers in order and can check whether one of them is "due"
	struct timer_queue{
		typedef boost::shared_ptr<deadline_timer> deadline_timer_ptr;
		template<class T>
		struct ptr_less{
			bool operator()(const boost::shared_ptr<T>& lhs, const boost::shared_ptr<T>& rhs)const{
				return *lhs < *rhs;
			}
		};
		
		/// stores all deadline_timers in order
		std::priority_queue<deadline_timer_ptr, std::vector<deadline_timer_ptr>, ptr_less<deadline_timer> > queue;      

		/**
		 * returns the number of microseconds to wait before next timeout
		 *
		 * this can be passed to zmq_poll directly.
		 */
		inline
		long next_timeout(){ 
			if(queue.empty())
				return -1;
			return queue.top()->get_timeout().total_microseconds(); }

		/**
		 * fires all events which are due currently
		 */
		inline 
		void fire_due(zmq_reactor::reactor* r){
			if(queue.empty()) return;
			boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
			while(!queue.empty() && queue.top()->is_due(now)){
				queue.top()->fire(r);
				queue.pop();
			}
		}
	};
}

#endif /* __GPF_TIMER_HPP__ */
