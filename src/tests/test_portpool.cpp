#include <gtest/gtest.h>

#include <gpf/util/portpool.hpp>

TEST(portpool_test, init){
	gpf::portpool pp(5);
	EXPECT_EQ(5, pp.get(1)[0]);
	EXPECT_EQ(7, pp.get(2)[1]);
}

