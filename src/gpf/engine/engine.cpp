#include <boost/thread.hpp>
#include <gpf/controller/heartmonitor.hpp>
#include <gpf/serialization/protobuf.hpp>
#include <gpf/messages/hub.pb.h>
#include <gpf/engine/engine.hpp>
#include <gpf/util/url_handling.hpp>

using namespace gpf;

engine::engine(zmq::context_t& ctx)
:m_registered(false),
 m_ctx(ctx)
{

}
void 
engine::dispatch_registration(zmq::socket_t& s){
	ZmqMessage::Incoming<ZmqMessage::SimpleRouting> in(s);
	in.receive_all();
	auto it = in.begin<std::string>();

	if(*it=="registration_request" ){
		LOG_IF(FATAL, *++it!="ACK")<<"Engine `"<<m_queue_name<<"' did NOT received registration request ACK";
		m_registered = true;
	}
}

void
engine::run(const std::string& name, const engine_info& ei){
	m_heart_name = name + "-heart";
	m_queue_name = name + "-queue";

	m_heart.reset(new heart(ei.heartbeat[0], ei.heartbeat[1], m_heart_name));
	boost::thread t(&heart::operator(), m_heart.get());
	m_queue.reset(new zmq::socket_t (m_ctx, ZMQ_REP));
	m_queue->setsockopt(ZMQ_IDENTITY, m_queue_name.c_str(), m_queue_name.size());


	// say hello to the hub
	LOG_IF(FATAL,!validate_url(ei.hub_registration));
	m_hub_registration.reset(new zmq::socket_t (m_ctx, ZMQ_REQ));
	m_reactor.add(*m_hub_registration,    ZMQ_POLLIN, boost::bind(&engine::dispatch_registration,this, _1));
	m_hub_registration->connect(ei.hub_registration.c_str());
	ZmqMessage::Outgoing<ZmqMessage::SimpleRouting> out(*m_hub_registration,0);
	gpf_hub::registration msg;
	msg.set_heartbeat(m_heart_name);
	msg.set_queue(m_queue_name);
	BOOST_FOREACH(const std::string& s, m_services)
		msg.add_services(s);
	out << "registration_request" << m_header_marshal(msg)<<ZmqMessage::Flush;

	m_reactor.run();
}

engine&
engine::provide_service(const std::string& s){
	m_services.push_back(s);
	return *this;
}
void 
engine::shutdown(){
	LOG(INFO)<<"Engine shutting down.";
	// send logoff message to hub
	ZmqMessage::Outgoing<ZmqMessage::SimpleRouting> out(*m_hub_registration,0); 
	try{
	out << "unregistration_request"<<m_queue_name;
	}catch(...){
		LOG(INFO)<<"Could not unregister at hub, already dead?";
	}
	m_heart->shutdown();
	m_reactor.shutdown();
}
