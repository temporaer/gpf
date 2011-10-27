#include <boost/format.hpp>

#include <glog/logging.h>

#include <gpf/controller/hub.hpp>

using namespace gpf;
using boost::format;


hub::hub()
{
}
hub::~hub()
{
}

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
	std::string client_iface = str(format("%s://%s:")
			%m_client_transport
			%m_client_ip) + "%i";

	std::string engine_iface = str(format("%s://%s:")
			%m_engine_transport
			%m_engine_ip) + "%i";

	zmq::socket_t q (m_ctx, ZMQ_ROUTER);
	q.bind((format(client_iface)%m_reg_port).str().c_str());

	LOG(INFO) << "Hub listening on `"<<format(client_iface)%m_reg_port
		<< "' for registration.";

	if(m_client_ip != m_engine_ip){
		q.bind((format(engine_iface)%m_reg_port).str().c_str());
		LOG(INFO) << "Hub listening on `"<<format(engine_iface)%m_reg_port
			<< "' for registration.";
	}

	// Heartbeat
	zmq::socket_t hpub(m_ctx, ZMQ_PUB);
	hpub.bind(str(format(engine_iface)%m_hb_ports[0]).c_str());
	zmq::socket_t hrep(m_ctx, ZMQ_ROUTER);
	hrep.bind(str(format(engine_iface)%m_hb_ports[1]).c_str());
	

	// client connections
	// notifier socket
	zmq::socket_t n(m_ctx, ZMQ_PUB);
	n.bind((format(client_iface)%m_notifier_port).str().c_str());

	// build and launch queues
	zmq::socket_t sub(m_ctx, ZMQ_SUB);
	sub.setsockopt(ZMQ_SUBSCRIBE, "", 0);
	sub.bind(m_monitor_url.c_str());
	sub.bind("inproc://monitor");

	// build info structs
	client_info ci;
	ci.control      = str(format(client_iface) % m_control_ports[0]);
	ci.mux          = str(format(client_iface) % m_mux_ports[0]);
	ci.iopub        = str(format(client_iface) % m_iopub_ports[0]);
	ci.notification = str(format(client_iface) % m_notifier_port);
	ci.task         = str(format(client_iface) % m_task_ports[0]);
	ci.task_scheme  = "default";

	engine_info ei;
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
		<< "     task    " <<ei.task<<std::endl;

	LOG(INFO)<<"Hub client addrs :"<<std::endl
		<< "     control " <<ci.control<<std::endl
		<< "     mux     " <<ci.mux<<std::endl
		<< "     iopub   " <<ci.iopub<<std::endl
		<< "     notify  " <<ci.notification<<std::endl
		<< "     task    " <<ci.task<<std::endl
		<< "     task_scheme    " <<ci.task_scheme<<std::endl;

	// resubmit stream
	zmq::socket_t r(m_ctx, ZMQ_DEALER);
	const char* session_name = "session-name";
	r.setsockopt(ZMQ_IDENTITY, session_name, strlen(session_name));
	r.connect(ci.task.c_str());
	return boost::shared_ptr<hub>();
}
