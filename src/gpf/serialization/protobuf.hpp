#ifndef __GPF_PROTOBUF_HPP__
#     define __GPF_PROTOBUF_HPP__
#include <gpf/serialization/common.hpp>

namespace gpf
{
	namespace serialization
	{
		struct protobuf_archive{};
		template<>
		struct archive_type_traits<protobuf_archive>{
			template<class T>
			static
			void serialize(std::string& s, const T& msg){
				std::ostringstream ss;
				msg.SerializeToString(&s);
			}

			template<class T>
			static
			int deserialize(T& msg, const std::string& s){
				if(!msg.ParseFromString(s)){
					LOG(ERROR)<<"ProtoBuf: Failed to deserialize msg: "<<s;
					return -1;
				}
				return 0;
			}
		};
	}
}

#endif /* __GPF_PROTOBUF_HPP__ */
