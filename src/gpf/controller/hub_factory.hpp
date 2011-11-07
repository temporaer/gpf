#ifndef __HUB_FACTORY_HPP__
#     define __HUB_FACTORY_HPP__

#include <gpf/controller/hub.hpp>

namespace gpf
{

	class hub_factory{
		public:
			boost::shared_ptr<hub> get();

			hub_factory& ip(const std::string&);
			hub_factory& transport(const std::string&);
			hub_factory& hm_interval(int millisecs);

			hub_factory(int startport);


		private:
			portpool m_portpool;

			zmq::context_t m_ctx;

			int m_heartmonitor_interval_millisec;

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
			boost::shared_ptr<heartmonitor> m_heartmonitor;

			// reactor
			zmq_reactor::reactor m_reactor;
	};

}

#endif /* __HUB_FACTORY_HPP__ */
