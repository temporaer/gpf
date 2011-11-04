#ifndef __GPF_CLIENT_HPP
#     define __GPF_CLIENT_HPP

#include <boost/shared_ptr.hpp>
#include <gpf/util/zmqmessage.hpp>
#include <gpf/controller/hub.hpp>
#include <zmq-poll-wrapper/reactor.hpp>


namespace gpf
{
	class client{
		public:
			client(zmq::context_t& ctx);

			void run(const client_info& ci);

			inline zmq_reactor::reactor& get_loop(){ return m_reactor; }
			
		private:
			zmq::context_t&      m_ctx;
			zmq_reactor::reactor m_reactor;
	};
}

#endif /* __GPF_CLIENT_HPP */
