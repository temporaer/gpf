#ifndef __PORTPOOL_HPP__
#     define __PORTPOOL_HPP__

#include <set>
#include <vector>
#include <vector>

namespace gpf
{
	typedef int port_t;
	typedef std::vector<port_t> ports_t;

	class portpool{
		private:
			port_t m_begin;
			port_t m_end;
		public:
			portpool(int start):m_begin(start),m_end(start){}
			inline ports_t get(int n){
				std::vector<port_t> v;
				for (int i = 0; i < n; ++i)
					v.push_back(m_end++);
				return v;
			}
	};
	
}

#endif /* __PORTPOOL_HPP__ */
