#ifndef __GPF_EXCEPT_HPP__
#     define __GPF_EXCEPT_HPP__

#include <boost/exception/all.hpp>

typedef boost::error_info<struct tag_engine_name,std::string> engine_name_t;
struct unknown_engine_error
:virtual boost::exception, virtual std::exception{ };
#endif /* __GPF_EXCEPT_HPP__ */
