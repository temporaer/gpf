#ifndef __GPF_CONTROLLER_DB_HPP__
#     define __GPF_CONTROLLER_DB_HPP__

#include <google/protobuf/message.h>
#include <map>

namespace gpf
{

	template<class K, class V>
	class dict_db{
		private:
			std::map<K,V> m_db;
		public:
			typedef typename std::map<K,V>::iterator             iterator;
			typedef typename std::map<K,V>::const_iterator const_iterator;

			      V& operator[](const K&k)       { return m_db[k]; }
			const V& operator[](const K&k) const { return m_db[k]; }

			      iterator find(const K&k)       { return m_db.find(k); }
			const_iterator find(const K&k) const { return m_db.find(k); }

			      iterator begin()     { return m_db.begin(); }
			const_iterator begin()const{ return m_db.begin(); }
			      iterator   end()     { return m_db.end(); }
			const_iterator   end()const{ return m_db.end(); }

			void update(const K&k, const V& v){
				iterator it = find(k);
				if(it==end()) throw std::runtime_error("No such key");
				throw std::runtime_error("Not implemented");
				//const google::protobuf::Message::Reflection* r = v.GetReflection();
				//std::vector<google::protobuf::FieldDescriptor*> fds;
				//r->ListFields(&v,&fds);
			}


			
	};
	
}

#endif /* __GPF_CONTROLLER_DB_HPP__ */
