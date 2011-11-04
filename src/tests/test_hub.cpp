#include <gtest/gtest.h>

#include <gpf/controller/hub.hpp>
#include <gpf/engine/engine.hpp>

TEST(hub_test, hub_init){
	gpf::hub_factory hf(5000);
	hf.ip("127.0.0.1").transport("tcp");
	hf.get();
}

TEST(hub_test, engine_init){
	gpf::hub_factory hf(5000);
	hf.ip("127.0.0.1").transport("tcp");

	boost::shared_ptr<gpf::hub> p( hf.get() );
	gpf::engine_info ei = p->get_engine_info();

	zmq::context_t ctx(1);
	gpf::engine engine(ctx);
}
