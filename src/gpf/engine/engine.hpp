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
		private:
			zmq::context_t&      m_ctx;
			zmq_reactor::reactor m_reactor;
			serialization::serializer<serialization::protobuf_archive> m_header_marshal;
	};
}

#endif /* __GPF_ENGINE_HPP__ */
