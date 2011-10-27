#ifndef __CONTROLLER_HPP__
#     define __CONTROLLER_HPP__

#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>
#include <string>

#include <zmq.hpp>
#include <zmq_utils.h> 
#include <gpf/util/portpool.hpp>

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

	struct engine_info
	{
		std::string control;
		std::string mux;
		std::string task;
		std::string iopub;
		std::string heartbeat[2];
	};

	struct client_info
	{
		std::string control;
		std::string mux;
		std::string task;
		std::string task_scheme;
		std::string iopub;
		std::string notification;
	};

	class hub_factory{
		public:
			boost::shared_ptr<hub> get();

			hub_factory& ip(const std::string&);
			hub_factory& transport(const std::string&);

			hub_factory(int startport);


		private:
			portpool m_portpool;

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
			ports_t m_hb_ports;     // XREQ/SUB Port pair for Engine heartbeats
			ports_t m_mux_ports;     // Engine/Client Port pair for MUX queue
			ports_t m_task_ports;    // Engine/Client Port pair for Task queue
			ports_t m_control_ports; // Engine/Client Port pair for Control queue
			ports_t m_iopub_ports;   // Engine/Client Port pair for IOPub relay
			port_t m_mon_port;      // Monitor (UB) port for queue traffic
			port_t m_notifier_port; // PUB port for sending engine status notifications
			port_t m_reg_port;      // For registration

			// heart monitor


	};
	
};

#endif /* __CONTROLLER_HPP__ */
