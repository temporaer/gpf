#include <boost/thread.hpp>
#include <gpf/controller/heartmonitor.hpp>
#include <gpf/engine/engine.hpp>

using namespace gpf;

engine::engine(zmq::context_t& ctx)
:m_ctx(ctx)
{

}

void
engine::run(const std::string& name, const engine_info& ei){
	heart h(ei.heartbeat[0], ei.heartbeat[1], name+"-heart");
	boost::thread t(&heart::operator(), &h);
	m_reactor.run();
}
