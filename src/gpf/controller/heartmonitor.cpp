#include <algorithm>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <glog/logging.h>
#include <gpf/util/zmqmessage.hpp>
#include "heartmonitor.hpp"

using namespace gpf;
using boost::posix_time::microsec_clock;
using boost::posix_time::ptime;
using boost::posix_time::from_iso_string;
using boost::posix_time::to_iso_string;
using boost::posix_time::time_duration;


heart::heart(const std::string& in_addr, const std::string& out_addr,
		std::string heart_id,
		int in_type, int out_type)
: 
	m_id(heart_id),
	m_in_addr(in_addr),
	m_out_addr(out_addr),
	m_in_type(in_type),
	m_out_type(out_type),
	m_ctx(1),
	m_count(0)
{
}

void 
heart::bumm(zmq::socket_t& sub, zmq::socket_t* rep){
	m_count ++;
	ZmqMessage::Incoming<ZmqMessage::SimpleRouting> in(sub);
	ZmqMessage::Outgoing<ZmqMessage::XRouting> out(
			ZmqMessage::OutOptions(*rep,
				ZmqMessage::OutOptions::NONBLOCK |  ZmqMessage::OutOptions::DROP_ON_BLOCK),
			in);
	in.receive_all();
	out << m_id;
	for (unsigned int i = 0; i < in.size(); ++i)
		out << in[i];
	out << ZmqMessage::Flush;
}

void 
heart::operator()(){
	zmq::socket_t sub(m_ctx, m_in_type);
	zmq::socket_t rep(m_ctx, m_out_type);
	rep.connect(m_out_addr.c_str());
	sub.connect(m_in_addr.c_str());
	if(m_in_type==ZMQ_SUB)
		sub.setsockopt(ZMQ_SUBSCRIBE, "", 0);

	if(m_id.size()==0)
		m_id = boost::lexical_cast<std::string>(boost::uuids::random_generator()());

	rep.setsockopt(ZMQ_IDENTITY, m_id.c_str(), m_id.size());

	VLOG(2)<<"Heart:: Heart `"<<m_id<<"' running.";
	
	zmq_reactor::reactor<> r;
	r.add(sub, ZMQ_POLLIN, boost::bind(&heart::bumm,this,_1,&rep));

	while(1) r(10);
}

heartmonitor::heartmonitor(heartmonitor::loop_type& loop, boost::shared_ptr<zmq::socket_t> pub, boost::shared_ptr<zmq::socket_t> router, int interval)
:
 m_interval(interval)
,m_pub(pub)
,m_router(router)
,m_lifetime (microsec_clock::universal_time())
,m_tic      (microsec_clock::universal_time())
,m_last_ping(microsec_clock::universal_time())
{
	loop.add(*m_router,ZMQ_POLLIN, boost::bind(&heartmonitor::handle_pong,this, _1)); // register w/ loop

	m_pub   ->setsockopt(ZMQ_IDENTITY, "heartMonitor_pub",strlen("heartMonitor_pub"));
	m_router->setsockopt(ZMQ_IDENTITY, "heartMonitor_rout",strlen("heartMonitor_rout"));
	loop.add(gpf::deadline_timer(boost::posix_time::milliseconds(m_interval), boost::bind(&heartmonitor::beat,this,_1)));
}
void heartmonitor::handle_pong (zmq::socket_t& s){
	// a heart just beat (s==m_router)
	
	ZmqMessage::Incoming<ZmqMessage::XRouting> msg(s);
	msg.receive_all();
	
	std::string   heart = ZmqMessage::get_string(msg[0]);
	std::string current = to_iso_string(m_lifetime);
	std::string    last = to_iso_string(m_last_ping);
	std::string received= ZmqMessage::get<std::string>(msg[1]);
	if(received == current){
		time_duration delta = microsec_clock::universal_time() - m_lifetime;
		m_responses.insert(heart);
		VLOG(2) << "Heartbeat::Heart `"<<heart<<"' responded in time, took "<<delta<<" to respond.";
	}else if(received == last){
		time_duration delta = microsec_clock::universal_time() - m_last_ping;
		VLOG(2) << "Heartbeat::Heart `"<<heart<<"' missed a beat, and took "<<delta<<" to respond.";
		m_responses.insert(heart);
	}else{
		VLOG(2) << "Heartbeat::Got bad heartbeat (possibly old): "<<last<<" current: "<<current;
	}
}

void heartmonitor::beat(heartmonitor::loop_type* r){
	// re-register callback
	r->add(gpf::deadline_timer(boost::posix_time::milliseconds(m_interval), boost::bind(&heartmonitor::beat,this,_1)));

	VLOG(2)<<"Heartmonitor:: beating.";
	m_last_ping = m_lifetime;
	ptime toc   = microsec_clock::universal_time();
	m_lifetime += toc - m_tic;
	m_tic       = toc;

	std::set<std::string> goodhearts, missed_beats, heartfailures, newhearts;
       	std::set_intersection(m_hearts.begin(),m_hearts.end(),
			m_responses.begin(),m_responses.end(),
			std::inserter(goodhearts,goodhearts.begin()));
	std::set_difference(m_hearts.begin(),m_hearts.end(),
		goodhearts.begin(), goodhearts.end(),
		std::inserter(missed_beats,missed_beats.begin()));
	std::set_intersection(m_on_probation.begin(),m_on_probation.end(),
		missed_beats.begin(), missed_beats.end(),
		std::inserter(heartfailures,heartfailures.begin()));
	std::set_difference(m_responses.begin(), m_responses.end(),
		goodhearts.begin(),goodhearts.end(),
		std::inserter(newhearts,newhearts.begin()));

	BOOST_FOREACH(const std::string& s, newhearts)
		handle_new_heart(s);
	BOOST_FOREACH(const std::string& s, heartfailures)
		handle_heart_failure(s);

	m_on_probation.clear();
	std::set_intersection(missed_beats.begin(),missed_beats.end(),
		m_hearts.begin(),m_hearts.end(),
		std::inserter(m_on_probation,m_on_probation.begin()));
	m_responses.clear();

	ZmqMessage::Outgoing<ZmqMessage::SimpleRouting> msg(*m_pub,
			ZmqMessage::OutOptions::NONBLOCK | ZmqMessage::OutOptions::DROP_ON_BLOCK);
	msg << to_iso_string(m_lifetime)<<ZmqMessage::Flush;
}

void heartmonitor::handle_new_heart(const std::string& heart){
	VLOG(2) << "Heartbeat: got new heart `"<<heart<<"'";
	// TODO: register and call handlers
	m_hearts.insert(heart);
}
void heartmonitor::handle_heart_failure(const std::string& heart){
	LOG(WARNING) << "Heartbeat: heart `"<<heart<<"' just failed!";
	// TODO: register and call handlers
	m_hearts.erase(heart);
}
