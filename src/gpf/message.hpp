#ifndef __GPF_MESSAGE_HPP__
#     define __GPF_MESSAGE_HPP__
#include <string>
#include <map>

namespace gpf
{
	struct query_message_header
	{
		std::string msg_id;
		std::string msg_type;
		template<class Archive>
		void serialize(Archive& ar, const unsigned int version){
			ar & msg_id & msg_type;
		}
	};

	struct registration_message
	{
		std::string heartbeat; ///< the name of the heart of an engine
		std::string queue;     ///< the name of the queue of an engine

		template<class Archive>
		void serialize(Archive& ar, const unsigned int version){
			ar & heartbeat & queue;
		}
	};

}

#endif /* __GPF_MESSAGE_HPP__ */
