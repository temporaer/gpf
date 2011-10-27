#ifndef __CONTROLLER_HPP__
#     define __CONTROLLER_HPP__

#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>
#include <string>

#include <zmq.hpp>
#include <zmq_utils.h> 

namespace gpf{

	class hub
	: boost::noncopyable
	{
	public:
		hub( );
		~hub();
		
	private:
		// session: Session object
		// queue: ZMQStream for monitoring the command queue (SUB)
		// query: ZMQStream for engine registration and client queries requests (XREP)
		// heartbeat: HeartMonitor object checking the pulse of the engines
		// notifier: ZMQStream for broadcasting engine registration changes (PUB)
		// engine_info: dict of zmq connection information for engines to connect
		// to the queues.
		// client_info: dict of zmq connection information for engines to connect
		// to the queues.
	};

	class hub_factory{
		public:
			boost::shared_ptr<hub> get();

			hub_factory& ip(const std::string&);
			hub_factory& transport(const std::string&);

		private:

			zmq::context_t m_ctx;

			// monitor
			std::string m_monitor_transport;
			std::string m_monitor_ip;

			std::string m_monitor_url;
			void _update_monitor_url();

			// engine
			std::string m_engine_transport;
			std::string m_engine_ip;
			
			// client
			std::string m_client_transport;
			std::string m_client_ip;

			// ports
			int m_mon_port;
			int m_notifier_port;
			int m_reg_port;


			
	};
	
};

#endif /* __CONTROLLER_HPP__ */
