#ifndef __GPF_TASK_SET_HPP__
#     define __GPF_TASK_SET_HPP__

#include <string>
#include <vector>
#include <set>
#include <boost/shared_ptr.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <gpf/util/zmqmessage.hpp>

namespace gpf
{

	struct task{
		std::string id;
		int         client_uuid;
		int         engine_uuid;
		std::string  queue; // e.g. mux, task
		mutable boost::posix_time::ptime submitted;
		mutable boost::posix_time::ptime   started;
		mutable boost::posix_time::ptime completed;
		mutable boost::posix_time::ptime resubmitted;

		mutable std::string  content;
		mutable std::string  result_content;

		mutable boost::shared_ptr<ZmqMessage::Incoming<ZmqMessage::XRouting> > incoming_msg; ///< request
		mutable boost::shared_ptr<ZmqMessage::Incoming<ZmqMessage::XRouting> > outgoing_msg; ///< reply
	};


	typedef boost::multi_index::multi_index_container<
		task,
		boost::multi_index::indexed_by<
			// sort by ID
			boost::multi_index::ordered_unique<boost::multi_index::tag<struct task_id_t>,boost::multi_index::member<task,std::string,&task::id> >,
			// sort by client ID
			boost::multi_index::ordered_non_unique<boost::multi_index::tag<struct client_id_t>,boost::multi_index::member<task,int,&task::client_uuid> >,
			// sort by engine ID
			boost::multi_index::ordered_non_unique<boost::multi_index::tag<struct engine_id_t>,boost::multi_index::member<task,int,&task::engine_uuid> >
		> 
	> task_set;

	struct task_tracker{
		task_set          tasks;
		std::set<std::string> pending;
		std::set<std::string> all_completed;
		std::set<std::string> unassigned;
	};

}

#endif /* __GPF_TASK_SET_HPP__ */

