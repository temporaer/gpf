#include <string>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <functional>
#include <boost/lexical_cast.hpp>
#include <glog/logging.h>
#include "url_handling.hpp"

using namespace std;
namespace gpf{

url::url(const std::string& url_s){
	parse(url_s);
}
void url::parse(const string& url_s)
{
    const string prot_end("://");
    if(url_s.size()<=prot_end.size())
	    throw std::runtime_error("Url Parser: URL too short!");
    string::const_iterator prot_i = search(url_s.begin(), url_s.end(),
                                           prot_end.begin(), prot_end.end());
    m_protocol.reserve(distance(url_s.begin(), prot_i));
    transform(url_s.begin(), prot_i,
              back_inserter(m_protocol),
              ptr_fun<int,int>(tolower)); // protocol is icase
    if( prot_i == url_s.end() ){
	    throw std::runtime_error("Url Parser: Could not find protocol string :// in `"+ url_s+ "'");
    }
    advance(prot_i, prot_end.length());
    string::const_iterator port_i = find(prot_i, url_s.end(), ':');
    string::const_iterator path_i = find(prot_i, url_s.end(), '/');

    string::const_iterator  host_end;

    if(port_i!=url_s.end()){
	    std::string portstr;
	    advance(port_i, 1); // move across ":"
	    portstr.reserve(distance(port_i,path_i));
	    copy(port_i, path_i, back_inserter(portstr)); 
	    m_port = boost::lexical_cast<int>(portstr);
	    host_end = port_i -1;
    }
    else {
	    m_port = -1;
	    host_end = path_i;
    }


    m_host.reserve(distance(prot_i, host_end));

    transform(prot_i, host_end,
              back_inserter(m_host),
              ptr_fun<int,int>(tolower)); // host is icase
    string::const_iterator query_i = find(path_i, url_s.end(), '?');
    m_path.assign(path_i, query_i);
    if( query_i != url_s.end() )
        ++query_i;
    m_query.assign(query_i, url_s.end());

    if(m_host.size()==0)
	   throw std::runtime_error("URL parsing error: `"+url_s+"'");
    if(m_host.find("/")!=std::string::npos)
	   throw std::runtime_error("URL parsing error: `"+url_s+"'");
}


bool validate_url(const std::string& u){
	try{
		gpf::url tmp(u);
	}catch(...){
		return false;
	}
	return true;
}

}
