#include <boost/thread.hpp>

#include <gtest/gtest.h>

#include <gpf/controller/heartmonitor.hpp>


struct body{
	gpf::heart heart;

	body(std::string id)
	:heart("tcp://127.0.0.1:5555", "tcp://127.0.0.1:5556", "heart"+id)
	{
	}

	void run(){
		heart();
	}
};

TEST(heartmonitor_test, init){
	typedef zmq_reactor::reactor loop_type;

	zmq::context_t ctx(1);
	loop_type loop(ctx);
	boost::shared_ptr<zmq::socket_t>
	   pub (new zmq::socket_t (ctx, ZMQ_PUB)),
	   rep (new zmq::socket_t (ctx, ZMQ_ROUTER));
	pub->bind("tcp://127.0.0.1:5555");
	rep->bind("tcp://127.0.0.1:5556");
	gpf::heartmonitor hm(loop, pub, rep, 400);

	body b("b");
	body c("c"), d("d");
	boost::thread t_b(&body::run, &b);
	boost::thread t_c(&body::run, &c);
	boost::thread t_d(&body::run, &d);

	// turn off after 10 seconds
	loop.add(gpf::deadline_timer(boost::posix_time::milliseconds(2100), boost::bind(&gpf::heart::shutdown, &b.heart)));
	loop.add(gpf::deadline_timer(boost::posix_time::milliseconds(2100), boost::bind(&gpf::heart::shutdown, &c.heart)));
	loop.add(gpf::deadline_timer(boost::posix_time::milliseconds(2100), boost::bind(&gpf::heart::shutdown, &d.heart)));
	loop.add(gpf::deadline_timer(boost::posix_time::milliseconds(2200), boost::bind(&loop_type::shutdown, &loop, "loop shutdown")));

	loop.run();

	EXPECT_EQ(b.heart.count(), 5);
	EXPECT_EQ(c.heart.count(), 5);
	EXPECT_EQ(d.heart.count(), 5);

	EXPECT_TRUE(hm.alive("heartb"));
	EXPECT_TRUE(hm.alive("heartc"));
	EXPECT_TRUE(hm.alive("heartd"));

}

