#include <gtest/gtest.h>

#include <gpf/controller/hub.hpp>

TEST(hub_test, init){
	gpf::hub_factory hf(5000);
	hf.ip("127.0.0.1").transport("tcp");
	hf.get();
}

