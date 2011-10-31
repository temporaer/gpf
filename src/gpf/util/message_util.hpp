#ifndef __MESSAGE_UTIL_HPP__
#     define __MESSAGE_UTIL_HPP__
#include <vector>
#include <string>
#include <gpf/util/zmqmessage.hpp>
#include <gpf/message.hpp>

namespace gpf
{
	namespace serialization
	{
		template<class T>
		void   serialize(std::string& ser, const T& obj){ ser = ""; }
		template<class T>
		void deserialize(T& obj, const std::string& ser){ ; }
	}
	namespace util
	{
		void feed_identities(std::vector<std::string>& idents, int& msg_start, ZmqMessage::Incoming<ZmqMessage::SimpleRouting>& ids);

		void deserialize(message& msg, int msg_start, ZmqMessage::Incoming<ZmqMessage::SimpleRouting>& inc);

	}
}

#endif /* __MESSAGE_UTIL_HPP__ */
