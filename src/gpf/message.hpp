#ifndef __GPF_MESSAGE_HPP__
#     define __GPF_MESSAGE_HPP__
#include <string>
#include <map>

	struct message
	{
		std::map<std::string,std::string> header;
		std::string msg_id;
		std::string msg_type;
		std::string parent_header; // or map????
		std::string content;
		// std::vector<std::string> buffers ???????
	};


#endif /* __GPF_MESSAGE_HPP__ */
