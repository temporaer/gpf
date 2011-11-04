#include <stdexcept>
#include <gtest/gtest.h>
#include <glog/logging.h>

#include <gpf/serialization.hpp>
#include <gpf/serialization/text.hpp>
#include <gpf/serialization/binary.hpp>
#include <gpf/serialization/protobuf.hpp>
#include <gpf/messages/test.pb.h>

struct msg_t{
	std::string s;
	int i;
	template<class Archive>
	void serialize(Archive& ar, const unsigned int version){
		ar & s & i;
	}
};

TEST(serialization_text_test, init){
	msg_t msg;
	msg.s = "hello world";
	msg.i = 42;

	std::string ser;
	gpf::serialization::serializer<gpf::serialization::text_archive> marshal;
	marshal.serialize(ser,msg);

	msg_t msg2;
	marshal.deserialize(msg2,ser);
	EXPECT_EQ(msg.s,msg2.s);
	EXPECT_EQ(msg.i,msg2.i);
}

TEST(serialization_binary_test, init){
	msg_t msg;
	msg.s = "hello world";
	msg.i = 42;

	std::string ser;
	gpf::serialization::serializer<gpf::serialization::binary_archive> marshal;
	marshal.serialize(ser,msg);

	msg_t msg2;
	marshal.deserialize(msg2,ser);
	EXPECT_EQ(msg.s,msg2.s);
	EXPECT_EQ(msg.i,msg2.i);
}

TEST(serialization_protobuf_test, init){
	gpf_test::msg_t msg;
	msg.set_s("hello world");
	msg.set_i(42);

	std::string ser;
	gpf::serialization::serializer<gpf::serialization::protobuf_archive> marshal;
	marshal.serialize(ser,msg);

	gpf_test::msg_t msg2;
	marshal.deserialize(msg2,ser);
	EXPECT_EQ(msg.s(),msg2.s());
	EXPECT_EQ(msg.i(),msg2.i());
}
