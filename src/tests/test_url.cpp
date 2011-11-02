#include <stdexcept>
#include <gtest/gtest.h>
#include <glog/logging.h>

#include <gpf/util/url_handling.hpp>

TEST(url_handling_test, init){
	{
		gpf::url url("tcp://127.0.0.1:40/bla");
		EXPECT_EQ(url.protocol(), "tcp");
		EXPECT_EQ(url.host(), "127.0.0.1");
		EXPECT_EQ(url.path(), "/bla");
		EXPECT_EQ(url.query(), "");
		EXPECT_EQ(url.port(), 40);
	}
	{
		gpf::url url("tcp://127.0.0.1/bla");
		EXPECT_EQ(url.protocol(), "tcp");
		EXPECT_EQ(url.host(), "127.0.0.1");
		EXPECT_EQ(url.path(), "/bla");
		EXPECT_EQ(url.query(), "");
		EXPECT_EQ(url.port(), -1);
	}
	{
		gpf::url url("tcp://127.0.0.1:40");
		EXPECT_EQ(url.protocol(), "tcp");
		EXPECT_EQ(url.host(), "127.0.0.1");
		EXPECT_EQ(url.path(), "");
		EXPECT_EQ(url.query(), "");
		EXPECT_EQ(url.port(), 40);
	}
	{
		gpf::url url("tcp://127.0.0.1");
		EXPECT_EQ(url.protocol(), "tcp");
		EXPECT_EQ(url.host(), "127.0.0.1");
		EXPECT_EQ(url.path(), "");
		EXPECT_EQ(url.query(), "");
		EXPECT_EQ(url.port(), -1);
	}
	{
		EXPECT_THROW({gpf::url url("tcp///127.0.0.1");}, // malformated
		std::runtime_error);
	}
	{
		EXPECT_THROW({gpf::url url("tcp:///127.0.0.1");}, // malformated
		std::runtime_error);
	}
}

