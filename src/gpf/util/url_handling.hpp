#ifndef __URL_HANDLING_HPP__
#     define __URL_HANDLING_HPP__
#include <string>

namespace gpf
{
	struct url {
		url(const std::string& url_s);

		const std::string& protocol()const{return m_protocol;}
		const std::string&     host()const{return m_host;}
		const std::string&     path()const{return m_path;}
		const std::string&    query()const{return m_query;}
		const         int&     port()const{return m_port;}

		private:
		void parse(const std::string& url_s);
		private:
		std::string m_protocol, m_host, m_path, m_query;
		int m_port;
	};

	bool validate_url(const std::string& url);
}

#endif /* __URL_HANDLING_HPP__ */
