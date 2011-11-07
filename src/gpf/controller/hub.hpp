#ifndef __CONTROLLER_HPP__
#     define __CONTROLLER_HPP__

#include <boost/function.hpp>
#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/uuid/uuid.hpp>
#include <string>
#include <map>

#include <zmq.hpp>
#include <zmq_utils.h> 
#include <gpf/util/portpool.hpp>
#include <gpf/controller/heartmonitor.hpp>
#include <zmq-poll-wrapper/reactor.hpp>
#include <gpf/util/zmqmessage.hpp>
#include <gpf/serialization.hpp>
#include <gpf/controller/db.hpp>
#include <gpf/messages/hub.pb.h>
#include <gpf/except.hpp>
#include <gpf/controller/engine_set.hpp>
#include <gpf/controller/task_set.hpp>

namespace gpf{

	struct engine_info
	{
		std::string hub_registration;
		std::string control;
		std::string mux;
		std::string task;
		std::string iopub;
		std::string heartbeat[2];
	};

	struct client_info
	{
		std::string hub_registration;
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
				zmq_reactor::reactor& loop,
				boost::shared_ptr<zmq::socket_t> monitor,
				boost::shared_ptr<zmq::socket_t> query,
				boost::shared_ptr<zmq::socket_t> notifier,
				boost::shared_ptr<zmq::socket_t> resubmit,
				boost::shared_ptr<heartmonitor> hm,
				const engine_info& ei,
				const client_info& ci
		);
		~hub();

		const engine_info get_engine_info()const{return m_engine_info;}
		const client_info get_client_info()const{return m_client_info;}
		zmq_reactor::reactor& get_loop(){return m_loop;}

		const engine_connector& get_engine(int eid);
		const engine_connector& get_engine(const std::string& queue);
		unsigned int      get_num_engines();

		void run();
		void shutdown();
		
	private:
		void dispatch_monitor_traffic(zmq::socket_t&); // ME, IOPub and Task queue messages
		void dispatch_query(zmq::socket_t&); // requests from client

		// Heartbeat
		void handle_new_heart(const std::string& heart);
		void handle_heart_failure(const std::string& heart);

		typedef boost::shared_ptr<ZmqMessage::Incoming<ZmqMessage::XRouting> > incoming_msg_t;

		// MUX Queue Traffic
		void nop(incoming_msg_t); // TODO: should this func recv the msg nevertheless?
		void save_queue_request(incoming_msg_t);
		void save_queue_result (incoming_msg_t);

		// Task Queue Traffic
		void save_task_request(incoming_msg_t);
		void save_task_result(incoming_msg_t);
		void save_task_destination(incoming_msg_t);

		// IOPub traffic
		void save_iopub_message(incoming_msg_t);

		// registration requests
		void connection_request(incoming_msg_t);
		void register_engine(incoming_msg_t);
		void unregister_engine(incoming_msg_t);
		void finish_registration(const std::string& heart);

		void _handle_stranded_msgs(int eid, const std::string& uuid);
		void _purge_stalled_registration(const std::string& heart);
		void _unregister_engine( int eid );

		// client requests
		void shutdown_request(incoming_msg_t);
		void _shutdown();
		void check_load(incoming_msg_t);
		void queue_status(incoming_msg_t);
		void purge_results(incoming_msg_t);
		void resubmit_task(incoming_msg_t);
		//void _extract_record(incoming_msg_t);
		void get_results(incoming_msg_t);

		zmq_reactor::reactor&          m_loop;

		// sockets
		boost::shared_ptr<zmq::socket_t> m_monitor;
		boost::shared_ptr<zmq::socket_t> m_query;
		boost::shared_ptr<zmq::socket_t> m_notifier;
		boost::shared_ptr<zmq::socket_t> m_resubmit;

		typedef void (hub::*monitor_handler_t)(incoming_msg_t);
		typedef void (hub::*query_handler_t)(incoming_msg_t);
		std::map<std::string,monitor_handler_t> m_monitor_handlers;
		std::map<std::string,query_handler_t>   m_query_handlers;

		serialization::serializer<serialization::protobuf_archive> m_header_marshal;

		int m_registration_timeout;

		boost::shared_ptr<heartmonitor> m_heartmonitor;
		engine_tracker                  m_tracker;

		engine_info                    m_engine_info;
		client_info                    m_client_info;

		task_tracker                   m_ttracker;


	};
	
};

#endif /* __CONTROLLER_HPP__ */
