#ifndef __HEARTMONITOR_HPP__
#     define __HEARTMONITOR_HPP__

#include <set>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <zmq.hpp>
#include <zmq-poll-wrapper/reactor.hpp>

namespace gpf
{
	struct heart{
		std::string m_id;
		std::string m_in_addr;
		std::string m_out_addr;
		int         m_in_type;
		int         m_out_type;
		zmq::context_t m_ctx;
		long        m_count;
		// receives pings in a SUB socket
		// sends out pongs through a DEALER
		heart(const std::string& in_addr, const std::string& out_addr,
				std::string heart_id="",
				int in_type=ZMQ_SUB, int out_type=ZMQ_DEALER);
		void operator()();
		void bumm(zmq::socket_t&, zmq::socket_t* );
		inline long count(){ return m_count; }
	};
	class heartmonitor{
		typedef zmq_reactor::reactor loop_type;

		// sends out pings over a PUB socket
		// receives pongs through a ROUTER

		public:
		/// construct a heartmonitor
		heartmonitor(loop_type& loop, boost::shared_ptr<zmq::socket_t> pub, boost::shared_ptr<zmq::socket_t> route, int interval=2000);

		/// a heart just beat
		void handle_pong (zmq::socket_t& );

		/// send out pings and re-register callbacks
		void beat(loop_type*);

		/// a new heart sent a beat
		void handle_new_heart(const std::string&);

		/// a heart failed to send a beat
		void handle_heart_failure(const std::string&);

		private:

		int m_interval; ///< in milliseconds

		boost::shared_ptr<zmq::socket_t> m_pub;    // outbound traffic
		boost::shared_ptr<zmq::socket_t> m_router; // inbound traffic

		boost::posix_time::ptime m_lifetime;
		boost::posix_time::ptime m_tic;
		boost::posix_time::ptime m_last_ping;

		std::set<std::string> m_hearts;
		std::set<std::string> m_responses;
		std::set<std::string> m_on_probation;
	};
}
#endif /* __HEARTMONITOR_HPP__ */
