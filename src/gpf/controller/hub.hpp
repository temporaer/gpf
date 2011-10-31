#ifndef __CONTROLLER_HPP__
#     define __CONTROLLER_HPP__

#include <boost/function.hpp>
#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>
#include <string>
#include <map>

#include <zmq.hpp>
#include <zmq_utils.h> 
#include <gpf/util/portpool.hpp>
#include <gpf/controller/heartmonitor.hpp>
#include <zmq-poll-wrapper/reactor.hpp>
#include <gpf/util/zmqmessage.hpp>
#include <gpf/message.hpp>

namespace gpf{
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


	class hub
	: boost::noncopyable
	{
	public:
		hub( 
				zmq_reactor::reactor<>& m_loop,
				boost::shared_ptr<zmq::socket_t> monitor,
				boost::shared_ptr<zmq::socket_t> query,
				boost::shared_ptr<zmq::socket_t> notifier,
				boost::shared_ptr<zmq::socket_t> resubmit,
				engine_info ei,
				client_info ci
		);
		~hub();
		
	private:
		void dispatch_monitor_traffic(zmq::socket_t&); // ME, IOPub and Task queue messages
		void dispatch_query(zmq::socket_t&); // requests from client

		// Heartbeat
		void handle_new_heart(zmq::socket_t&);
		void handle_heart_failure(zmq::socket_t&);

		typedef ZmqMessage::Incoming<ZmqMessage::SimpleRouting> incoming_msg_t;

		// MUX Queue Traffic
		void nop(std::vector<std::string>&, int msg_start, incoming_msg_t&); // TODO: should this func recv the msg nevertheless?
		void save_queue_request(std::vector<std::string>&, int msg_start, incoming_msg_t&);
		void save_queue_result (std::vector<std::string>&, int msg_start, incoming_msg_t&);

		// Task Queue Traffic
		void save_task_request(std::vector<std::string>&, int msg_start, incoming_msg_t&);
		void save_task_result(std::vector<std::string>&, int msg_start, incoming_msg_t&);
		void save_task_destination(std::vector<std::string>&, int msg_start, incoming_msg_t&);

		// IOPub traffic
		void save_iopub_message(std::vector<std::string>&, int msg_start, incoming_msg_t&);

		// registration requests
		void connection_request(std::vector<std::string>&,int msg_start,incoming_msg_t&);
		void register_engine(std::vector<std::string>&,int msg_start,incoming_msg_t&);
		void unregister_engine(std::vector<std::string>&,int msg_start,incoming_msg_t&);
		void _handle_stranded_msgs(const std::string& eid, const std::string& uuid);
		void finish_registration(const std::string& heart);
		void _purge_stalled_registration(const std::string& heart);

		// client requests
		void shutdown_request(std::vector<std::string>&,int msg_start,incoming_msg_t&);
		void _shutdown();
		void check_load(std::vector<std::string>&,int msg_start,incoming_msg_t&);
		void queue_status(std::vector<std::string>&,int msg_start,incoming_msg_t&);
		void purge_results(std::vector<std::string>&,int msg_start,incoming_msg_t&);
		void resubmit_task(std::vector<std::string>&,int msg_start,incoming_msg_t&);
		//void _extract_record(std::vector<std::string>&,int msg_start,incoming_msg_t&);
		void get_results(std::vector<std::string>&,int msg_start,incoming_msg_t&);
		void db_query(std::vector<std::string>&,int msg_start,incoming_msg_t&);
		void get_history(std::vector<std::string>&,int msg_start,incoming_msg_t&);

		typedef void (hub::*monitor_handler_t)(std::vector<std::string>&, int msg_start, incoming_msg_t&);
		typedef void (hub::*query_handler_t)(std::vector<std::string>&,int msg_start,incoming_msg_t&);
		std::map<std::string,monitor_handler_t> m_monitor_handlers;
		std::map<std::string,query_handler_t>   m_query_handlers;


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
			boost::shared_ptr<heartmonitor> m_heartmonitor;

			// reactor
			zmq_reactor::reactor<> m_reactor;
	};
	
};

#endif /* __CONTROLLER_HPP__ */
