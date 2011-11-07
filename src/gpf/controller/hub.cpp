#include <boost/bind.hpp>

#include <glog/logging.h>

#include <gpf/controller/hub.hpp>
#include <gpf/util/url_handling.hpp>

#include <gpf/serialization/protobuf.hpp>
#include <gpf/messages/hub.pb.h>

using namespace gpf;


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

void hub::finish_registration(const std::string& heart){
	// Second half of engine registration, called after our HeartMonitor
	// has received a beat from the Engine's Heart.
	
	auto& index = m_tracker.incoming_registrations.get<heart_name_t>();
	auto it = index.find(heart);
	if(it == index.end()) {
		LOG(ERROR) << "Trying to finish non-existent registration of heart `"<<heart<<"'";
		return;
	}
	const engine_connector& ec = *it;

	std::stringstream ss;
	std::copy(ec.services.begin(),ec.services.end(),std::ostream_iterator<std::string>(ss,", "));
	LOG(INFO)<<"Finishing registration of engine "<<ec.id<<":`"<<ec.queue<<"' ("<<ss.str()<<")";
	if(ec.deletion_callback)
		ec.deletion_callback->set_inactive();

	m_tracker.engines.insert(ec);

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
	auto& index = m_tracker.incoming_registrations.get<heart_name_t>();
	if(index.find(heart)==index.end())
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
	auto& index = m_tracker.engines.get<heart_name_t>();
	auto it = index.find(heart);
	if(it == index.end()){
		LOG(INFO) << "Hub:: ignoring heart failure";
		return;
	}
	_unregister_engine(it->id);
}

void hub::register_engine(incoming_msg_t incoming){
	// TODO: wrap errors in a message and send them back to client
	gpf_hub::registration msg;

	if(0!=m_header_marshal.deserialize(msg,*incoming->iter_at<std::string>(1)))
	        return;

	std::string queue = msg.queue();
	std::string heart = msg.heartbeat();

	int eid = m_tracker.next_id();
	DLOG(INFO)<<"Registration::register_engine "<<eid<<" "<<queue<<" "<<" "<<heart;

	auto& heart_index = m_tracker.engines.get<heart_name_t>();
	auto& queue_index = m_tracker.engines.get<queue_name_t>();
	auto& heart_index2 = m_tracker.incoming_registrations.get<heart_name_t>();
	auto& queue_index2 = m_tracker.incoming_registrations.get<queue_name_t>();
	if(queue_index.find(queue) != queue_index.end()) {
		LOG(ERROR)<<"Registration::queue id "<<msg.queue()<<" in use!"; return;
	}
	else if(heart_index.find(heart)!=heart_index.end()){
		LOG(ERROR)<<"Registration::heart id "<<msg.heartbeat()<<" in use!"; return;
	}
	else if(queue_index2.find(queue) != queue_index2.end()) {
		LOG(ERROR)<<"Registration::queue id "<<msg.queue()<<" in use!"; return;
	}
	else if(heart_index2.find(heart)!=heart_index2.end()){
		LOG(ERROR)<<"Registration::heart id "<<msg.heartbeat()<<" in use!"; return;
	}

	gpf::engine_connector ec;
	ec.id           = eid;
	ec.queue        = queue;
	ec.heartbeat    = heart;
	ec.services.reserve(msg.services_size());
	ec.incoming_msg = incoming;
	std::copy(msg.services().begin(),msg.services().end(),std::back_inserter(ec.services));
	ec.registration = msg.reg();

	if(m_heartmonitor->alive(heart)){
		LOG(INFO) << "heart is already beating, finish off registration";
		m_tracker.incoming_registrations.insert(ec);
		finish_registration(heart);
	}else{
		// heart is not beating, schedule for deletion (revokable if heartbeat comes in time)
		LOG(INFO) << "heart is not beating, wait for heartbeat to finish registration";
		boost::shared_ptr<deadline_timer> dt(
			new deadline_timer(boost::posix_time::milliseconds(m_registration_timeout),
				boost::bind(&hub::_purge_stalled_registration,this,heart)));
		ec.deletion_callback = dt;
		m_tracker.incoming_registrations.insert(ec);
		m_loop.add(dt);
	}

}
void hub::_purge_stalled_registration(const std::string& heart){
	auto& index = m_tracker.incoming_registrations.get<heart_name_t>();
	auto it = index.find(heart);
	if(it == index.end())
		return;
	DLOG(INFO)<<"Purging stalled registration: " << it->id;
	index.erase(it);
}

void hub::_unregister_engine( int eid ){
	// unregister an engine (due to its own request or due to heart failure)
	auto& index = m_tracker.engines.get<engine_id_t>();
	auto it = index.find(eid);
	if(it == index.end()){
		LOG(ERROR)<<"Trying to unregister unknown engine "<<eid;
		return;
	}
	m_tracker.dead_engines.insert(it->queue);
	m_loop.add(deadline_timer(boost::posix_time::milliseconds(m_registration_timeout),
				boost::bind(&hub::_handle_stranded_msgs, this, eid, it->queue)));

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
	auto& index = m_tracker.engines.get<engine_id_t>();
	auto it    = index.find(eid);
	if(it == index.end()){
		LOG(ERROR)<< "Handling stranded messages for unknown eid="<<eid;
		return;
	}
	
	const engine_connector& ec = *it;
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

const engine_connector& 
hub::get_engine(int eid){
	auto& index = m_tracker.engines.get<engine_id_t>();
	auto it = index.find(eid);
	if(it == index.end())
		throw unknown_engine_error() << engine_name_t(boost::lexical_cast<std::string>(eid));
	return *it;
}
const engine_connector& 
hub::get_engine(const std::string& queue){
	auto& index = m_tracker.engines.get<queue_name_t>();
	auto it = index.find(queue);
	if(it == index.end())
		throw unknown_engine_error() << engine_name_t(queue);
	return *it;
}
unsigned int      
hub::get_num_engines(){
	return m_tracker.engines.size();
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
void hub::check_load(incoming_msg_t incoming){
	gpf_hub::load_request inmsg;
	gpf_hub::load_reply   outmsg;
	if(0!=m_header_marshal.deserialize(inmsg,*incoming->iter_at<std::string>(1)))
	        return;
	auto& index = m_tracker.engines.get<engine_id_t>();
	bool ok = true;
	for(int i=0;i<inmsg.eid_size();i++){
		auto it = index.find(inmsg.eid(i));
		if(it==index.end()) {
			ok = false;
			break;
		}
		outmsg.add_queuelen(it->queues.size());
		outmsg.add_taskslen(it->tasks.size());
	}
	ZmqMessage::Outgoing<ZmqMessage::XRouting> out(*m_query,*incoming,0);
	out << "load_reply"<<m_header_marshal(outmsg);
}
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
	auto& index = m_tracker.engines.get<queue_name_t>();
	auto it     = index.find(queue);
	if(it == index.end()){
		LOG(ERROR)<<"Trying to unregister unknown engine `"<<queue<<"'";
		return;
	}
	LOG(INFO)<<"Unregistring engine "<<it->id<< " ("<<queue<<")";
	index.erase(it);
}

void hub::shutdown(){
	LOG(INFO)<<"Hub shutting down. #connected engines: "<<m_tracker.engines.size();
	m_loop.shutdown();
}
