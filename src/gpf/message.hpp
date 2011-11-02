#ifndef __GPF_MESSAGE_HPP__
#     define __GPF_MESSAGE_HPP__
#include <string>
#include <map>

namespace gpf
{
	struct message
	{
		std::map<std::string,std::string> header;
		std::string msg_id;
		std::string msg_type;
		std::string parent_header; // or map????
		std::string content;
		// std::vector<std::string> buffers ???????
	};

	struct registration_message
	{
		std::string heartbeat; // the name of the heart of an engine
		std::string queue;     // the name of the queue of an engine
		bool        ok;
	};

}

#endif /* __GPF_MESSAGE_HPP__ */
