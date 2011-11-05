#include <boost/thread.hpp>
#include <gpf/controller/heartmonitor.hpp>
#include <gpf/serialization/protobuf.hpp>
#include <gpf/messages/hub.pb.h>
#include <gpf/engine/engine.hpp>
#include <gpf/util/url_handling.hpp>

using namespace gpf;

engine::engine(zmq::context_t& ctx)
:m_ctx(ctx)
{

}

void
engine::run(const std::string& name, const engine_info& ei){
	std::string heart_name = name + "-heart";
	std::string queue_name = name + "-queue";

	heart h(ei.heartbeat[0], ei.heartbeat[1], heart_name);
	boost::thread t(&heart::operator(), &h);
	zmq::socket_t queue(m_ctx, ZMQ_REP);
	queue.setsockopt(ZMQ_IDENTITY, queue_name.c_str(), queue_name.size());

	// say hello to the hub
	LOG_IF(FATAL,!validate_url(ei.hub_registration));
	zmq::socket_t reg(m_ctx, ZMQ_REQ);
	reg.connect(ei.hub_registration.c_str());
	ZmqMessage::Outgoing<ZmqMessage::SimpleRouting> out(reg,0);
	gpf_hub::registration msg;
	msg.set_heartbeat(heart_name);
	msg.set_queue(queue_name);
	std::string query;
	LOG(INFO)<<"Engine: Registring at "<<ei.hub_registration;
	m_header_marshal.serialize(query, msg);
	out << "registration_request" << query<<ZmqMessage::Flush;

	LOG(ERROR)<<"running reactor of engine";

	m_reactor.run();
}
