#ifndef __GPF_ENGINE_HPP__
#     define __GPF_ENGINE_HPP__
#include <gpf/controller/hub.hpp>
#include <gpf/serialization.hpp>

namespace gpf
{
	class engine{
		public:
			engine(zmq::context_t& ctx);
			void run(const std::string& name, const engine_info& ei);
			inline zmq_reactor::reactor& get_loop(){ return m_reactor; }

			engine& provide_service(const std::string& s);
			void shutdown();
			void dispatch_registration(zmq::socket_t& s); 
			inline bool registered(){ return m_registered; }
		private:
			bool                         m_registered;
			std::string                  m_queue_name;
			std::string                  m_heart_name;
			std::auto_ptr<heart>         m_heart;
			boost::shared_ptr<zmq::socket_t> m_hub_registration; ///< registration communication with hub
			boost::shared_ptr<zmq::socket_t> m_queue; ///< my task queue
			std::vector<std::string> m_services; ///< a list of services this engine provides
			zmq::context_t&      m_ctx;          ///< reference to ZMQ context obj
			zmq_reactor::reactor m_reactor;      ///< dispatches events
			serialization::serializer<serialization::protobuf_archive> m_header_marshal; 
	};
}

#endif /* __GPF_ENGINE_HPP__ */
