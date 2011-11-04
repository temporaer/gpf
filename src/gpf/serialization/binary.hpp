#ifndef __GPF_SERIALIZATION_BINARY_HPP__
#     define __GPF_SERIALIZATION_BINARY_HPP__
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <gpf/serialization/common.hpp>

namespace gpf
{
	namespace serialization
	{
		struct binary_archive{};

		template<>
		struct archive_type_traits<binary_archive>{
			typedef boost::archive::binary_iarchive in_archive_t;
			typedef boost::archive::binary_oarchive out_archive_t;

			template<class T>
			static
			void serialize(std::string& s, const T& msg){
				std::ostringstream ss;
				{
					out_archive_t oa(ss);
					oa << msg;
				}
				s = ss.str();
			}

			template<class T>
			static
			int deserialize(T& msg, const std::string& s){
				try{
					std::istringstream ss(s);
					in_archive_t ia(ss);
					ia >> msg;
				}catch(boost::archive::archive_exception e){
					LOG(ERROR)<<"Failed to deserialize msg: "<<e.what();
					return -1;
				}
				return 0;
			}
		};
		
	}
}

#endif /* __GPF_SERIALIZATION_BINARY_HPP__ */
