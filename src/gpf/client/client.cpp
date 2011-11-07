#include <gpf/client/client.hpp>
#include <gpf/messages/hub.pb.h>

using namespace gpf;

client::client(zmq::context_t& ctx)
:m_ctx(ctx)
,m_reactor(ctx)
{
}

void 
client::run(const client_info& ci)
{
	m_reactor.run();
}
