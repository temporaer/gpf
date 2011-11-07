#include <boost/thread.hpp>

#include <gtest/gtest.h>

#include <zmq-poll-wrapper/reactor.hpp>

typedef zmq_reactor::reactor loop_type;
using namespace boost::posix_time;

int number_timeouts1 = 0;
int number_timeouts2 = 0;

void handle_timeout1(loop_type* l){
	number_timeouts1 += 1;
	LOG(INFO) << "Timeout 1 number " << number_timeouts1;
	EXPECT_LE(number_timeouts2, number_timeouts1);
}

void handle_timeout2(loop_type* l){
	number_timeouts2 += 1;
	LOG(INFO) << "Timeout 2 number " << number_timeouts2;
	EXPECT_LE(number_timeouts2, number_timeouts1);
}

void stop_requester(loop_type* l){
	l->shutdown();
}


TEST(timeout_test, init){
	zmq::context_t ctx(1);
	loop_type loop(ctx);

	gpf::deadline_timer dt1(milliseconds(100), handle_timeout1);
	gpf::deadline_timer dt2(milliseconds(200), handle_timeout2);

	loop.add(gpf::deadline_timer(milliseconds(100), handle_timeout1));
	loop.add(gpf::deadline_timer(milliseconds(200), handle_timeout2));
	loop.add(gpf::deadline_timer(milliseconds(200), handle_timeout1));
	loop.add(gpf::deadline_timer(milliseconds(400), handle_timeout2));

	loop.add(gpf::deadline_timer(milliseconds(500), stop_requester));

	loop.run();

	EXPECT_EQ(number_timeouts1, 2);
	EXPECT_EQ(number_timeouts2, 2);
}

