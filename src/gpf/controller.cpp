#include <boost/format.hpp>
#include <glog/logging.h>

#include <gpf/controller.hpp>

using namespace gpf;


hub::hub()
{
}
hub::~hub()
{
}


void
hub_factory::_update_monitor_url(){
	 m_monitor_url = (boost::format("%s://%s:%i")
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
	std::string client_iface = str(boost::format("%s://%s:")
			%m_client_transport
			%m_client_ip) + "%i";

	std::string engine_iface = str(boost::format("%s://%s:")
			%m_engine_transport
			%m_engine_ip) + "%i";

	zmq::socket_t q (m_ctx, ZMQ_ROUTER);
	q.bind((boost::format(client_iface)%m_reg_port).str().c_str());

	DLOG(INFO) << "Hub listening on `"<<boost::format(client_iface)%m_reg_port
		   << "' for registration.";

	if(m_client_ip != m_engine_ip){
		q.bind((boost::format(engine_iface)%m_reg_port).str().c_str());
		DLOG(INFO) << "Hub listening on `"<<boost::format(engine_iface)%m_reg_port
			   << "' for registration.";
	}

	// Heartbeat: TODO
	
	// client connections
	// notifier socket
	zmq::socket_t n(m_ctx, ZMQ_PUB);
	n.bind((boost::format(client_iface)%m_notifier_port).str().c_str());

	// build and launch queues
	zmq::socket_t sub(m_ctx, ZMQ_SUB);
	sub.setsockopt(ZMQ_SUBSCRIBE, "", 0);
	sub.bind(m_monitor_url.c_str());
	sub.bind("inproc://monitor");

	// resubmit stream
	zmq::socket_t r(m_ctx, ZMQ_DEALER);

}
