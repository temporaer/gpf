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

	m_loop.add(m_query,    ZMQ_POLLIN, boost::bind(&hub::dispatch_query,this, _1));
	m_loop.add(m_monitor,  ZMQ_POLLIN, boost::bind(&hub::dispatch_monitor_traffic,this, _1));
	m_loop.add(m_resubmit, ZMQ_POLLIN, [=](zmq::socket_t&){});

	m_monitor_handlers["in"]         = &hub::save_queue_request;
	m_monitor_handlers["out"]        = &hub::save_queue_result;
	m_monitor_handlers["intask"]     = &hub::save_task_request;
	m_monitor_handlers["outtask"]    = &hub::save_task_result;
	m_monitor_handlers["tracktask"]  = &hub::save_task_destination;
	m_monitor_handlers["incontrol"]  = &hub::nop;
	m_monitor_handlers["outcontrol"] = &hub::nop;

	m_query_handlers["queue_request"]          = &hub::queue_status;
	m_query_handlers["result_request"]         = &hub::get_results;
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
		m_ttracker.pending.erase(msg_id);
		m_ttracker.all_completed.insert(msg_id);

		auto& index = m_ttracker.tasks.get<task_id_t>();
		auto it = index.find(msg_id);
		if(it==index.end()) {
			LOG(ERROR)<<"DB error handling stranded message "<<msg_id;
			continue;
		}
		it->content     = "Engine died while running task `" + msg_id + "'";
		it->completed   = boost::posix_time::microsec_clock::universal_time();
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
void hub::save_queue_request(incoming_msg_t incoming){

	gpf_hub::in         inmsg;
	if(0!=m_header_marshal.deserialize(inmsg,*incoming->iter_at<std::string>(1)))
	        return;

	task t;
	t.id           = inmsg.msg_id();
	t.incoming_msg = incoming;
	t.engine_uuid  = inmsg.eid();
	t.submitted    = boost::posix_time::from_iso_string(inmsg.submitted());
	t.queue        = "mux";

	// TODO: it's possible that iopub arrived first (see ipython code...)
	m_ttracker.pending.insert(t.id);
	m_ttracker.tasks.insert(t);
}
void hub::save_queue_result (incoming_msg_t incoming){
	gpf_hub::out        inmsg;
	if(0!=m_header_marshal.deserialize(inmsg,*incoming->iter_at<std::string>(1)))
	        return;

	auto& index = m_ttracker.tasks.get<task_id_t>();
	auto it = index.find(inmsg.msg_id());
	if(it == index.end()){
		LOG(ERROR)<<"save_queue_result: Got result for non-existent task";
		return;
	}
	std::string msg_id = inmsg.msg_id();
	if(m_ttracker.pending.find(msg_id) != m_ttracker.pending.end()){
		m_ttracker.pending.erase(msg_id);
		m_ttracker.all_completed.insert(msg_id);
	}else if(m_ttracker.all_completed.find(msg_id) == m_ttracker.all_completed.end()){
		// it could be a result from a dead engine that died before delivering the result
		LOG(ERROR)<<"queue:: unknown message ID "<<msg_id<<" finished.";
	}
	// update record anyway, because the unregistration could have been premature
	it->completed    = boost::posix_time::from_iso_string(inmsg.completed());
	it->started      = boost::posix_time::from_iso_string(inmsg.started());
	it->outgoing_msg = incoming;
}
void hub::save_task_request(incoming_msg_t incoming){
	// save the submission of a task
	
	gpf_hub::intask        inmsg;
	if(0!=m_header_marshal.deserialize(inmsg,*incoming->iter_at<std::string>(1)))
	        return;

	bool has_eid = inmsg.has_eid();

	task t;
	t.id           = inmsg.msg_id();
	t.incoming_msg = incoming;
	t.engine_uuid  = has_eid ? inmsg.eid() : -1;
	t.submitted    = boost::posix_time::from_iso_string(inmsg.submitted());
	t.queue        = "task";

	// TODO: it's possible that iopub arrived first (see ipython code...)
	if(has_eid)
		m_ttracker.unassigned.insert(t.id);
	m_ttracker.pending.insert(t.id);
	m_ttracker.tasks.insert(t);
	
}
void hub::save_task_result(incoming_msg_t incoming){
	gpf_hub::outtask    inmsg;
	if(0!=m_header_marshal.deserialize(inmsg,*incoming->iter_at<std::string>(1)))
	        return;

	auto& index = m_ttracker.tasks.get<task_id_t>();
	auto it = index.find(inmsg.msg_id());
	if(it == index.end()){
		LOG(ERROR)<<"save_queue_result: Got result for non-existent task";
		return;
	}
	std::string msg_id = inmsg.msg_id();
	{
	  auto it = m_ttracker.unassigned.find(msg_id);
	  if(m_ttracker.unassigned.end() != it)
		  m_ttracker.unassigned.erase(it);
	}

	if(m_ttracker.pending.find(msg_id) != m_ttracker.pending.end()){
		m_ttracker.pending.erase(msg_id);
		m_ttracker.all_completed.insert(msg_id);
	}else if(m_ttracker.all_completed.find(msg_id) == m_ttracker.all_completed.end()){
		// it could be a result from a dead engine that died before delivering the result
		LOG(ERROR)<<"queue:: unknown message ID "<<msg_id<<" finished.";
	}
	// update record anyway, because the unregistration could have been premature
	it->completed    = boost::posix_time::from_iso_string(inmsg.completed());
	it->started      = boost::posix_time::from_iso_string(inmsg.started());
	it->outgoing_msg = incoming;
	if(inmsg.has_eid()){
		if(inmsg.eid() != it->engine_uuid){
			task t = *it;
			t.engine_uuid = inmsg.eid();
			index.replace(it,t);
		}
	}
}
void hub::save_task_destination(incoming_msg_t incoming){
	gpf_hub::tracktask    inmsg;
	if(0!=m_header_marshal.deserialize(inmsg,*incoming->iter_at<std::string>(1)))
	        return;

	auto& index = m_ttracker.tasks.get<task_id_t>();
	auto it = index.find(inmsg.msg_id());
	if(it == index.end()){
		LOG(ERROR)<<"save_task_destination: Got msg for non-existent task";
		return;
	}
	std::string msg_id = inmsg.msg_id();
	{
	  auto it = m_ttracker.unassigned.find(msg_id);
	  if(m_ttracker.unassigned.end() != it)
		  m_ttracker.unassigned.erase(it);
	}

	if(inmsg.eid() != it->engine_uuid){
		task t = *it;
		t.engine_uuid = inmsg.eid();
		index.replace(it,t);
	}
	LOG(INFO)<<"Task "<<msg_id<<" arrived on "<<inmsg.eid();

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
		outmsg.add_eid(inmsg.eid(i));
		outmsg.add_queuelen(it->queues.size());
		outmsg.add_taskslen(it->tasks.size());
	}
	if(ok){
		ZmqMessage::Outgoing<ZmqMessage::XRouting> out(*m_query,*incoming,0);
		out << "load_reply"<<m_header_marshal(outmsg);
	}else{
		ZmqMessage::Outgoing<ZmqMessage::XRouting> out(*m_query,*incoming,0);
		out << "load_reply"<<"Error: Got unknown ID";
	}
}
void hub::queue_status(incoming_msg_t incoming){
	// Return the Queue status of one or more targets.
	// if verbose: return the msg_ids
	// else: return len of each type.
	// keys: queue (pending MUX jobs)
	//       tasks (pending Task jobs)
	//       completed (finished jobs from both queues)
	gpf_hub::queue_status_request inmsg;
	gpf_hub::queue_status_reply   outmsg;
	if(0!=m_header_marshal.deserialize(inmsg,*incoming->iter_at<std::string>(1)))
	        return;
	auto& index = m_tracker.engines.get<engine_id_t>();
	bool  ok = true;
	bool  verbose = inmsg.verbose();
	for(int i=0;i<inmsg.eids_size();i++){
		auto it = index.find(inmsg.eids(i));
		if(it==index.end()) {
			ok = false;
			break;
		}
		gpf_hub::queue_status_reply::status& smsg = *outmsg.add_engine();

		auto& tindex = m_ttracker.tasks.get<engine_id_t>();

		int task_cnt=0, compl_cnt=0;
		BOOST_FOREACH(const task& t, tindex.equal_range(inmsg.eids(i))){
			if(verbose){
				if(t.completed.is_not_a_date_time()){
					smsg.add_tasks(t.id);
					task_cnt++;
				}
				else{
					smsg.add_completed(t.id);
					compl_cnt++;
				}
			}
		}
		
		smsg.set_eid(inmsg.eids(i));
		smsg.set_taskslen(task_cnt);
	}
	if(ok){
		ZmqMessage::Outgoing<ZmqMessage::XRouting> out(*m_query,*incoming,0);
		out << "queue_status_reply"<<m_header_marshal(outmsg);
	}else{
		ZmqMessage::Outgoing<ZmqMessage::XRouting> out(*m_query,*incoming,0);
		out << "queue_status_reply"<<"Error: Got unknown ID";
	}
}
void hub::purge_results(incoming_msg_t incoming){
	gpf_hub::purge_results_request inmsg;
	gpf_hub::purge_results_reply   outmsg;
	if(0!=m_header_marshal.deserialize(inmsg,*incoming->iter_at<std::string>(1)))
	        return;
	if(inmsg.all()){
		m_ttracker.tasks.clear();
	}else{
		auto& id_index = m_ttracker.tasks.get<task_id_t>();
		// purge messages from database
		for(int i=0;i<inmsg.msg_ids_size();i++){
			std::string id = inmsg.msg_ids(i);
			if(m_ttracker.pending.find(id) != m_ttracker.pending.end()){
				ZmqMessage::Outgoing<ZmqMessage::XRouting> out(*m_query,*incoming,0);
				out << "purge_results_reply"<< format("Error: Got pending msg_id %s")%id;
				return;
			}
			auto it = id_index.find(inmsg.msg_ids(i));
			if(it!=id_index.end())
				id_index.erase(it);
		}

		// purge messages from queue of specific engines
		auto& tindex = m_tracker.engines.get<engine_id_t>();
		for(int i=0;i<inmsg.eids_size();i++){
			auto  p = tindex.equal_range(inmsg.eids(i));
			tindex.erase(p.first, p.second);
		}
	}
	ZmqMessage::Outgoing<ZmqMessage::XRouting> out(*m_query,*incoming,0);
	out << "load_reply"<<m_header_marshal(outmsg);
}
void hub::resubmit_task(incoming_msg_t){}
void hub::get_results(incoming_msg_t incoming){
	gpf_hub::get_results_request inmsg;
	gpf_hub::get_results_reply   outmsg;
	if(0!=m_header_marshal.deserialize(inmsg,*incoming->iter_at<std::string>(1)))
	        return;

	auto& id_index = m_ttracker.tasks.get<task_id_t>();
	bool status_only = inmsg.status_only();

	std::vector<std::string*> content_bufs;
	for(int i=0;i<inmsg.msg_ids_size();i++){
		std::string id = inmsg.msg_ids(i);
		auto it = id_index.find(inmsg.msg_ids(i));
		if(it==id_index.end()){
			ZmqMessage::Outgoing<ZmqMessage::XRouting> out(*m_query,*incoming,0);
			out << "get_results_reply"<<format("Unknown message id %s")%id;
			return;
		}
		gpf_hub::get_results_reply::result& res = *outmsg.add_results();
		res.set_msg_id(id);
		res.set_status(it->completed.is_not_a_date_time() 
				? gpf_hub::get_results_reply::PENDING 
				: gpf_hub::get_results_reply::COMPLETED);
		if(!status_only)
			content_bufs.push_back(&it->result_content);
	}

	ZmqMessage::Outgoing<ZmqMessage::XRouting> out(*m_query,*incoming,0);
	out << "load_reply"<<m_header_marshal(outmsg);
	BOOST_FOREACH(std::string* s, content_bufs)
		out << *s;
}

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
