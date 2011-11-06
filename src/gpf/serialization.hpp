#ifndef __GPF_SERIALIZATION_HPP__
#     define __GPF_SERIALIZATION_HPP__

#include <gpf/serialization/common.hpp>

namespace gpf
{
	namespace serialization
	{
		template<typename atype>
		struct serializer{
			template<class T>
			void serialize(std::string& s, const T& msg){
				archive_type_traits<atype>::serialize(s,msg);
			}
			template<class T>
			std::string operator()(const T& msg){
				std::string s;
				archive_type_traits<atype>::serialize(s,msg);
				return s;
			}

			template<class T>
			int deserialize(T& msg, const std::string& s){
				return archive_type_traits<atype>::deserialize(msg,s);
			}
		};

		/// forward declaration for convenience
		struct text_archive;
		/// forward declaration for convenience
		struct binary_archive;
		/// forward declaration for convenience
		struct protobuf_archive;
	}
}

#endif /* __GPF_SERIALIZATION_HPP__ */
