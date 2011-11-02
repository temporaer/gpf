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
	loop_type loop;
	boost::shared_ptr<zmq::socket_t>
	   pub (new zmq::socket_t (ctx, ZMQ_PUB)),
	   rep (new zmq::socket_t (ctx, ZMQ_ROUTER));
	pub->bind("tcp://127.0.0.1:5555");
	rep->bind("tcp://127.0.0.1:5556");
	gpf::heartmonitor hm(loop, pub, rep);

	body b("b");
	body c("c"), d("d");
	boost::thread t_b(&body::run, &b);
	boost::thread t_c(&body::run, &c);
	boost::thread t_d(&body::run, &d);

	for (int i = 0; i < 1000; ++i)
	{
		hm.beat();
		loop(10000);
		loop(10000);
		loop(10000);
		loop(10000);
		loop(10000);
		loop(10000);
		boost::this_thread::sleep(boost::posix_time::seconds(1)); 
	}

	boost::this_thread::sleep(boost::posix_time::seconds(5)); 
}

