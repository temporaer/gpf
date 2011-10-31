#include <sstream>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/map.hpp>
#include "message_util.hpp"

namespace gpf { namespace util {
	typedef boost::archive::text_iarchive input_archive;
	typedef boost::archive::text_oarchive output_archive;

	static const std::string DELIM = "<IDS|MSG>";

	void feed_identities(std::vector<std::string>& idents, int& msg_start, ZmqMessage::Incoming<ZmqMessage::SimpleRouting>& msg){
		unsigned int i=0;
		for(i=0; i<msg.size(); i++){
			std::string part = ZmqMessage::get_string(msg[i]);
			if(part == DELIM){
				msg_start = i+1;
				break;
			}
			idents.push_back(part);
		}
		if(i>msg.size()){
			idents.clear();
			msg_start = 0;
		}
	}
	void deserialize(message& msg, int msg_start, ZmqMessage::Incoming<ZmqMessage::SimpleRouting>& inc){

		// inc[0] == signature...check...
		std::stringstream m1(ZmqMessage::get_string(inc[msg_start+1]));
		input_archive(m1) >> msg.header;
		msg.msg_id = msg.header["msg_id"];
		msg.msg_type = msg.header["msg_type"];
		std::stringstream m2(ZmqMessage::get_string(inc[msg_start+2]));
		input_archive(m2) >> msg.parent_header;
		std::stringstream m3(ZmqMessage::get_string(inc[msg_start+3]));
		input_archive(m3) >> msg.content;

		// buffers = ...
	}

} }
