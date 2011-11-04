#ifndef __MESSAGE_UTIL_HPP__
#     define __MESSAGE_UTIL_HPP__
#include <vector>
#include <string>
#include <gpf/util/zmqmessage.hpp>
#include <gpf/message.hpp>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/type_traits/is_same.hpp>

namespace gpf
{
	namespace util
	{
		//void feed_identities(std::vector<std::string>& idents, int& msg_start, ZmqMessage::Incoming<ZmqMessage::SimpleRouting>& ids);
		//void deserialize(query_message& msg, int msg_start, ZmqMessage::Incoming<ZmqMessage::SimpleRouting>& inc);

		struct text_archive{};
		struct binary_archive{};
		struct protobuf_archive{};

		template<typename atype>
		struct archive_type_traits{
			BOOST_STATIC_ASSERT(!(boost::is_same<atype,atype>::type));
		};
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
		template<>
		struct archive_type_traits<text_archive>{
			typedef boost::archive::text_iarchive in_archive_t;
			typedef boost::archive::text_oarchive out_archive_t;

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


		template<typename atype>
		struct serializer{
			template<class T>
			void serialize(std::string& s, const T& msg){
				archive_type_traits<atype>::serialize(s,msg);
			}

			template<class T>
			int deserialize(T& msg, const std::string& s){
				return archive_type_traits<atype>::deserialize(msg,s);
			}
		};

		
	}
}

#endif /* __MESSAGE_UTIL_HPP__ */
