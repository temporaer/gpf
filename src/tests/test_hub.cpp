#include <gtest/gtest.h>

#include <boost/bind.hpp>
#include <boost/thread.hpp>
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
	int timeout_secs = 5;

	boost::shared_ptr<gpf::hub> hub = hf.get();
	// schedule shutdown in a second
	hub->get_loop().add(gpf::deadline_timer(boost::posix_time::seconds(timeout_secs),
				boost::bind(&zmq_reactor::reactor::shutdown,&hub->get_loop())));

	zmq::context_t ctx(1);
	gpf::engine engine(ctx);
	engine.get_loop().add(gpf::deadline_timer(boost::posix_time::seconds(timeout_secs),
	                        boost::bind(&zmq_reactor::reactor::shutdown,&engine.get_loop())));

	boost::thread engine_thread([&](){engine.run("engine0", hub->get_engine_info());});


	hub->run();
	engine_thread.join();
}
