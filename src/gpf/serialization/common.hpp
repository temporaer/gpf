#ifndef __GPF_SERIALIZATION_COMMON_HPP__
#     define __GPF_SERIALIZATION_COMMON_HPP__
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_same.hpp>

namespace gpf
{
	namespace serialization
	{
		template<typename atype>
		struct archive_type_traits{
			BOOST_STATIC_ASSERT(!(boost::is_same<atype,atype>::type));
		};
	}
}

#endif /* __GPF_SERIALIZATION_COMMON_HPP__ */
