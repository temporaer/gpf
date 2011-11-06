#include <boost/bind.hpp>
#include <boost/format.hpp>

#include <glog/logging.h>

#include <gpf/controller/hub.hpp>
#include <gpf/util/url_handling.hpp>

#include <gpf/serialization/protobuf.hpp>
#include <gpf/messages/hub.pb.h>

using namespace gpf;
using boost::format;


// Hub constructor
hub::hub(
				zmq_reactor::reactor& loop,
				boost::shared_ptr<zmq::socket_t> monitor,
				boost::shared_ptr<zmq::socket_t> query,
				boost::shared_ptr<zmq::socket_t> notifier,
				boost::shared_ptr<zmq::socket_t> resubmit,
				boost::shared_ptr<heartmonitor> hm,
				const engine_info& ei,
				const client_info& ci
		)
:
m_loop(loop)
,m_monitor(monitor)
,m_query(query)
,m_notifier(notifier)
,m_resubmit(resubmit)
,m_heartmonitor(hm)
,m_engine_info(ei)
,m_client_info(ci)
{
	m_registration_timeout =  std::max(5000, 2*hm->interval());

	m_loop.add(*m_query,    ZMQ_POLLIN, boost::bind(&hub::dispatch_query,this, _1));
	m_loop.add(*m_monitor,  ZMQ_POLLIN, boost::bind(&hub::dispatch_monitor_traffic,this, _1));
	m_loop.add(*m_resubmit, ZMQ_POLLIN, [=](zmq::socket_t&){});

	m_monitor_handlers["in"]         = &hub::save_queue_request;
	m_monitor_handlers["out"]        = &hub::save_queue_result;
	m_monitor_handlers["intask"]     = &hub::save_task_request;
	m_monitor_handlers["outtask"]    = &hub::save_task_result;
	m_monitor_handlers["tracktask"]  = &hub::save_task_destination;
	m_monitor_handlers["incontrol"]  = &hub::nop;
	m_monitor_handlers["outcontrol"] = &hub::nop;
	m_monitor_handlers["iopub"]      = &hub::save_iopub_message;

	m_query_handlers["queue_request"]          = &hub::queue_status;
	m_query_handlers["result_request"]         = &hub::get_results;
	m_query_handlers["history_request"]        = &hub::get_history;
	m_query_handlers["db_request"]             = &hub::db_query;
	m_query_handlers["purge_request"]          = &hub::purge_results;
	m_query_handlers["load_request"]           = &hub::check_load;
	m_query_handlers["resubmit_request"]       = &hub::resubmit_task;
	m_query_handlers["shutdown_request"]       = &hub::shutdown_request;
	m_query_handlers["registration_request"]   = &hub::register_engine;
	m_query_handlers["unregistration_request"] = &hub::unregister_engine;
	m_query_handlers["connection_request"]     = &hub::connection_request;

	
	hm->register_new_heart_handler(   boost::bind(&hub::handle_new_heart    ,this,_1));
	hm->register_failed_heart_handler(boost::bind(&hub::handle_heart_failure,this,_1));

	DLOG_IF(FATAL, !validate_url(ci.hub_registration));
	DLOG_IF(FATAL, !validate_url(ci.control));
	DLOG_IF(FATAL, !validate_url(ci.mux));
	DLOG_IF(FATAL, !validate_url(ci.task));
	DLOG_IF(FATAL, !validate_url(ci.iopub));
	DLOG_IF(FATAL, !validate_url(ci.notification));

	DLOG_IF(FATAL, !validate_url(ei.hub_registration));
	DLOG_IF(FATAL, !validate_url(ei.control));
	DLOG_IF(FATAL, !validate_url(ei.mux));
	DLOG_IF(FATAL, !validate_url(ei.task));
	DLOG_IF(FATAL, !validate_url(ei.iopub));
	DLOG_IF(FATAL, !validate_url(ei.heartbeat[0]));
	DLOG_IF(FATAL, !validate_url(ei.heartbeat[1]));

	LOG(INFO)<<"hub::created hub";
}

void hub::run(){
	m_loop.run();
}

int hub::next_id(){
	static int id = 0;
	return id++;
}
void hub::finish_registration(const std::string& heart){
	// Second half of engine registration, called after our HeartMonitor
	// has received a beat from the Engine's Heart.
	
	auto it = m_incoming_registrations.find(heart);
	if(it == m_incoming_registrations.end()) {
		LOG(ERROR) << "Trying to finish non-existent registration of heart `"<<heart<<"'";
		return;
	}
	const engine_connector& ec = it->second;

	std::stringstream ss;
	std::copy(ec.services.begin(),ec.services.end(),std::ostream_iterator<std::string>(ss,", "));
	LOG(INFO)<<"Finishing registration of engine "<<ec.id<<":`"<<ec.queue<<"' ("<<ss.str()<<")";
	if(ec.deletion_callback)
		ec.deletion_callback->set_inactive();

	m_engines[ec.id] = ec;

	m_ids.insert(ec.id);
	m_by_ident[ec.queue] = ec.id;
	m_hearts[heart]      = ec.id;

	gpf_hub::registration msg;
	msg.set_heartbeat(ec.heartbeat);
	msg.set_queue    (ec.queue);
	msg.set_reg      (ec.registration);
	BOOST_FOREACH(const std::string& s, ec.services)
		msg.add_services(s);
	{
		ZmqMessage::Outgoing<ZmqMessage::XRouting> out(*m_notifier,0);
		out << "registration_request"<<m_header_marshal(msg);
	}{
		ZmqMessage::Outgoing<ZmqMessage::XRouting> out(*m_query,*ec.incoming_msg,0);
		out << "registration_request"<<"ACK";
	}

	LOG(INFO)<<"Engine connected: "<<ec.id;

}
void hub::handle_new_heart(const std::string& heart){
	DLOG(INFO) << "Handle new heart `"<<heart<<"'";
	if(m_incoming_registrations.find(heart)==m_incoming_registrations.end())
	{
		LOG(INFO) << "Ignoring new heart `"<<heart<<"'";
	}else{
		finish_registration(heart);
	}

}
void hub::handle_heart_failure(const std::string& heart){
	/* 
	 * handler to attach to heartbeater.  called when a previously
	 * registered heart fails to respond to beat request.  
	 * triggers unregistration.
	 */
	DLOG(INFO)<<"Hub::Handle heart failure for heart `"<<heart<<"'";
	auto hearts_id = m_hearts.find(heart);
	if(hearts_id == m_hearts.end()){
		LOG(INFO) << "Hub:: ignoring heart failure";
		return;
	}
	int eid = hearts_id->second;
	_unregister_engine(heart,eid);
}

void hub::register_engine(incoming_msg_t incoming){
	// TODO: wrap errors in a message and send them back to client
	gpf_hub::registration msg;

	if(0!=m_header_marshal.deserialize(msg,*incoming->iter_at<std::string>(1)))
	        return;

	std::string queue = msg.queue();
	std::string heart = msg.heartbeat();

	int eid = next_id();
	DLOG(INFO)<<"Registration::register_engine "<<eid<<" "<<queue<<" "<<" "<<heart;

	bool ok = true;
	if(m_by_ident.find(queue) != m_by_ident.end()) {
		LOG(ERROR)<<"Registration::queue id "<<msg.queue()<<" in use!"; ok = false;
	}
	else if(m_hearts.find(heart)!=m_hearts.end()){
		LOG(ERROR)<<"Registration::heart id "<<msg.heartbeat()<<" in use!"; ok = false;
	}else{
		for(std::map<std::string, gpf::engine_connector>::iterator it = m_incoming_registrations.begin();
			it!= m_incoming_registrations.end(); it++){
			if(it->first == heart){
				LOG(ERROR)<<"Registration::heart id "<<msg.heartbeat()<<" in use!"; ok = false;
				break;
			}else if(it->second.queue == queue){
				LOG(ERROR)<<"Registration::queue id "<<msg.queue()<<" in use!"; ok = false;
				break;
			}
		}
	}
	if(ok){
		gpf::engine_connector ec;
		ec.id    = eid;
		ec.queue = queue;
		ec.services.reserve(msg.services_size());
		ec.incoming_msg = incoming;
		std::copy(msg.services().begin(),msg.services().end(),std::back_inserter(ec.services));
		ec.registration = msg.reg();

		if(m_heartmonitor->alive(heart)){
			LOG(INFO) << "heart is already beating, finish off registration";
			m_incoming_registrations[heart] = ec;
			finish_registration(heart);
		}else{
			// heart is not beating, schedule for deletion (revokable if heartbeat comes in time)
			LOG(INFO) << "heart is not beating, wait for heartbeat to finish registration";
			boost::shared_ptr<deadline_timer> dt(
				new deadline_timer(boost::posix_time::milliseconds(m_registration_timeout),
					boost::bind(&hub::_purge_stalled_registration,this,heart)));
			ec.deletion_callback = dt;
			m_incoming_registrations[heart] = ec;
			m_loop.add(dt);
		}
	}

}
void hub::_purge_stalled_registration(const std::string& heart){
	auto it = m_incoming_registrations.find(heart);
	if(it == m_incoming_registrations.end())
		return;
	DLOG(INFO)<<"Purging stalled registration: " << it->second.id;
	m_incoming_registrations.erase(it);
}

void hub::_unregister_engine(const std::string& heart, int eid ){
	// unregister an engine (due to its own request or due to heart failure)
	auto it = m_engines.find(eid);
	if(it == m_engines.end()){
		LOG(ERROR)<<"Trying to unregister non-existing engine "<<heart<<" "<<eid;
		return;
	}
	m_dead_engines.insert(it->second.queue);
	m_loop.add(deadline_timer(boost::posix_time::milliseconds(m_registration_timeout),
				boost::bind(&hub::_handle_stranded_msgs, this, eid, it->second.queue)));

	ZmqMessage::Outgoing<ZmqMessage::XRouting> out(*m_notifier,0);
	out << "unregistration_request" << eid;
}
void hub::_handle_stranded_msgs(int eid, const std::string& uuid){
	/** 
	 * Handle messages known to be on an engine when the engine unregisters.
	 *
	 * It is possible that this will fire prematurely - that is, an engine
	 * will go down after completing a result, and the client will be
	 * notified that the result failed and later receive the actual result.
	 */
	engine_connector& ec = m_engines[eid];
	std::vector<std::string>& outstanding = ec.queues;
	BOOST_FOREACH( std::string& msg_id, outstanding ){
		m_pending.erase(msg_id);
		m_all_completed.insert(msg_id);

		auto it = m_db.find(msg_id);
		if(it==m_db.end()) {
			LOG(ERROR)<<"DB error handling stranded message "<<msg_id;
			continue;
		}
		it->second.set_content("Engine died while running task `" + msg_id + "'");
		it->second.set_completed(to_iso_string(boost::posix_time::microsec_clock::universal_time()));
		it->second.set_engine_uuid(ec.queue);
	}
}

void hub::dispatch_monitor_traffic(zmq::socket_t& s){
	// all ME and Task queue messages come through here, as well as
	// IOPub traffic.
	incoming_msg_t incoming(new ZmqMessage::Incoming<ZmqMessage::XRouting>(s));
	incoming->receive_all();
	std::string type = ZmqMessage::get<std::string>((*incoming)[0]);
	DLOG(INFO) << "Monitor traffic : "<< ZmqMessage::get<std::string>((*incoming)[0]);
	
	std::map<std::string,monitor_handler_t>::iterator handler = m_monitor_handlers.find(type);
	if(handler == m_monitor_handlers.end()){
		LOG(ERROR) << "Invalid monitor topic: "<<type;
		return;
	}
	(this->*(handler->second))(incoming);
}

void hub::dispatch_query(zmq::socket_t&s){
	// Route registration requests and queries from clients.
	incoming_msg_t incoming(new ZmqMessage::Incoming<ZmqMessage::XRouting>(s));
	incoming->receive_all();
	
	std::string type = ZmqMessage::get<std::string>((*incoming)[0]);
	std::map<std::string,query_handler_t>::iterator handler = m_query_handlers.find(type);
	LOG(INFO) << "Incoming query: `"<<type<<"'";
	if(handler == m_query_handlers.end()){
		LOG(ERROR) << "Bad Message Type: "<<type;
		return;
	}
	(this->*(handler->second))(incoming);
}

hub::~hub()
{
}

void hub::nop(incoming_msg_t){
}
void hub::save_queue_request(incoming_msg_t){
	
}
void hub::save_queue_result (incoming_msg_t){
	
}
void hub::save_task_request(incoming_msg_t){
	
}
void hub::save_task_result(incoming_msg_t){
	
}
void hub::save_task_destination(incoming_msg_t){
	
}

void hub::shutdown_request(incoming_msg_t){}
void hub::_shutdown(){}
void hub::check_load(incoming_msg_t){}
void hub::queue_status(incoming_msg_t){}
void hub::purge_results(incoming_msg_t){}
void hub::resubmit_task(incoming_msg_t){}
void hub::get_results(incoming_msg_t){}
void hub::db_query(incoming_msg_t){}
void hub::get_history(incoming_msg_t){}

void hub::save_iopub_message(incoming_msg_t){}
void hub::connection_request(incoming_msg_t){}

void hub::unregister_engine(incoming_msg_t in){
	std::string queue = ZmqMessage::get<std::string>((*in)[1]);
	auto it = m_by_ident.find(queue);
	if(it == m_by_ident.end()){
		LOG(ERROR)<<"Trying to unregister unknown engine `"<<queue<<"'";
	}
	int eid = it->second;
	m_engines.erase(eid);
	LOG(INFO)<<"Unregistring engine "<<eid<< " ("<<queue<<")";
}

void hub::shutdown(){
	LOG(INFO)<<"Hub shutting down. #connected engines: "<<m_engines.size();
	m_loop.shutdown();
}

 /**********************************
  *         Hub Factory
  **********************************/

hub_factory::hub_factory(int startport)
:m_portpool(startport),
 m_ctx(1)
{
	m_hb_ports      = m_portpool.get(2);
	m_mux_ports     = m_portpool.get(2);
	m_task_ports    = m_portpool.get(2);
	m_control_ports = m_portpool.get(2);
	m_iopub_ports   = m_portpool.get(2);
	m_mon_port      = m_portpool.get(1)[0];
	m_notifier_port = m_portpool.get(1)[0];
	m_reg_port      = m_portpool.get(1)[0];
}

void
hub_factory::_update_monitor_url(){
	m_monitor_url = (format("%s://%s:%i")
			% m_monitor_transport
			% m_monitor_ip
			% m_mon_port).str();
}

hub_factory&
hub_factory::ip(const std::string& ip){
	m_monitor_ip = ip;
	m_client_ip  = ip;
	m_engine_ip  = ip;
	_update_monitor_url();
	return *this;
}

hub_factory&
hub_factory::transport(const std::string& transport){
	m_monitor_transport = transport;
	m_client_transport  = transport;
	m_engine_transport  = transport;
	_update_monitor_url();
	return *this;
}

boost::shared_ptr<hub>
hub_factory::get(){
	typedef boost::shared_ptr<zmq::socket_t> zmq_socket;

	std::string client_iface = str(format("%s://%s:")
			%m_client_transport
			%m_client_ip) + "%i";

	std::string engine_iface = str(format("%s://%s:")
			%m_engine_transport
			%m_engine_ip) + "%i";

	zmq_socket q( new zmq::socket_t(m_ctx, ZMQ_ROUTER) );
	q->bind((format(client_iface)%m_reg_port).str().c_str());

	LOG(INFO) << "Hub listening on `"<<format(client_iface)%m_reg_port
		<< "' for registration.";

	if(m_client_ip != m_engine_ip){
		q->bind((format(engine_iface)%m_reg_port).str().c_str());
		LOG(INFO) << "Hub listening on `"<<format(engine_iface)%m_reg_port
			<< "' for registration.";
	}


	// Heartbeat
	zmq_socket hpub (new zmq::socket_t(m_ctx, ZMQ_PUB) );
	hpub->bind(str(format(engine_iface)%m_hb_ports[0]).c_str());
	zmq_socket hrep (new zmq::socket_t(m_ctx, ZMQ_ROUTER) );
	hrep->bind(str(format(engine_iface)%m_hb_ports[1]).c_str());

	m_heartmonitor.reset(new heartmonitor(m_reactor, hpub, hrep));
	

	// client connections
	// notifier socket
	zmq_socket n( new zmq::socket_t (m_ctx, ZMQ_PUB) );
	n->bind((format(client_iface)%m_notifier_port).str().c_str());

	// build and launch queues
	zmq_socket sub( new zmq::socket_t(m_ctx, ZMQ_SUB) );
	sub->setsockopt(ZMQ_SUBSCRIBE, "", 0);
	sub->bind(m_monitor_url.c_str());
	sub->bind("inproc://monitor");

	// build info structs
	client_info ci;
	ci.hub_registration =  str(format(client_iface) % m_reg_port);
	ci.control      = str(format(client_iface) % m_control_ports[0]);
	ci.mux          = str(format(client_iface) % m_mux_ports[0]);
	ci.iopub        = str(format(client_iface) % m_iopub_ports[0]);
	ci.notification = str(format(client_iface) % m_notifier_port);
	ci.task         = str(format(client_iface) % m_task_ports[0]);
	ci.task_scheme  = "default";

	engine_info ei;
	ei.hub_registration =  str(format(engine_iface) % m_reg_port);
	ei.control = str(format(engine_iface) % m_control_ports[1]);
	ei.mux     = str(format(engine_iface) % m_mux_ports[1]);
	ei.iopub   = str(format(engine_iface) % m_iopub_ports[1]);
	ei.task    = str(format(engine_iface) % m_task_ports[1]);
	ei.heartbeat[0] = str(format(engine_iface) % m_hb_ports[0]);
	ei.heartbeat[1] = str(format(engine_iface) % m_hb_ports[1]);
	
	LOG(INFO)<<"Hub engine addrs :"<<std::endl
		<< "     control " <<ei.control<<std::endl
		<< "     mux     " <<ei.mux<<std::endl
		<< "     iopub   " <<ei.iopub<<std::endl
		<< "     heart0  " <<ei.heartbeat[0]<<std::endl
		<< "     heart1  " <<ei.heartbeat[1]<<std::endl
		<< "     task    " <<ei.task<<std::endl;

	LOG(INFO)<<"Hub client addrs :"<<std::endl
		<< "     control " <<ci.control<<std::endl
		<< "     mux     " <<ci.mux<<std::endl
		<< "     iopub   " <<ci.iopub<<std::endl
		<< "     notify  " <<ci.notification<<std::endl
		<< "     task    " <<ci.task<<std::endl
		<< "     task_scheme    " <<ci.task_scheme<<std::endl;

	// resubmit stream
	zmq_socket r(new zmq::socket_t(m_ctx, ZMQ_DEALER));
	const char* session_name = "session-name";
	r->setsockopt(ZMQ_IDENTITY, session_name, strlen(session_name));
	r->connect(ci.task.c_str());
	boost::shared_ptr<hub> H( new hub(m_reactor, sub, q, n, r,m_heartmonitor, ei,ci));

	return H;
}
