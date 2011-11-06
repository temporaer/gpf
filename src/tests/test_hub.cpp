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

template<class T>
void schedule_reactor_shutdown(int ms, T& t){
	t.get_loop().add(gpf::deadline_timer(boost::posix_time::milliseconds(ms),
				boost::bind(&zmq_reactor::reactor::shutdown,&t.get_loop())));
}

template<class T>
void schedule_obj_shutdown(int ms, T& t){
	t.get_loop().add(gpf::deadline_timer(boost::posix_time::milliseconds(ms),
				boost::bind(&T::shutdown,&t)));
}

TEST(hub_test, engine_init){
	gpf::hub_factory hf(5000);
	hf.ip("127.0.0.1").transport("tcp").hm_interval(200);

	boost::shared_ptr<gpf::hub> hub = hf.get();

	zmq::context_t ctx(1);
	gpf::engine engine(ctx);
	engine.provide_service("test-service1");
	engine.provide_service("test-service2");

	schedule_reactor_shutdown(500, *hub);
	schedule_reactor_shutdown(500, engine);

	boost::thread engine_thread([&](){engine.run("engine0", hub->get_engine_info());});

	hub->run();
	engine_thread.join();

	/***
	 * the engine should now be registered with the hub
	 ***/
	EXPECT_TRUE( engine.registered() );
	EXPECT_ANY_THROW( hub->get_engine("dummy-engine-queue") );
	EXPECT_NO_THROW(hub->get_engine("engine0-queue"));

	EXPECT_EQ(hub->get_num_engines(), 1);
	EXPECT_EQ(hub->get_engine("engine0-queue").services.size(), 2);
	EXPECT_EQ(hub->get_engine("engine0-queue").services[0], "test-service1");
	EXPECT_EQ(hub->get_engine("engine0-queue").services[1], "test-service2");


	/****
	 * continue execution
	 ****/

	schedule_reactor_shutdown(200, *hub);
	schedule_obj_shutdown(100, engine);

	boost::thread engine_thread2([&](){engine.get_loop().run();});

	hub->run();
	engine_thread2.join();

	/***
	 * test unregistration of engine
	 */

	EXPECT_ANY_THROW(hub->get_engine("engine0-queue"));

	EXPECT_EQ(hub->get_num_engines(), 0);

}
