#ifndef __GPF_ENGINE_SET_HPP__
#     define __GPF_ENGINE_SET_HPP__

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

	struct engine_connector{
		int id; ///< engine id
		std::string queue; ///< zmq socket identity
		mutable std::string registration; ///< ???
		mutable std::string control; ///< ??? from `queue'
		mutable std::string heartbeat; ///< heart's name
		mutable std::vector<std::string> services; ///< services this engine offers

		// stuff littering the hub class in ipython
		mutable std::vector<std::string> queues; ///< ???
		mutable std::vector<std::string> tasks; ///< ???
		mutable std::vector<std::string> completed; ///< ???

		mutable boost::shared_ptr<ZmqMessage::Incoming<ZmqMessage::XRouting> > incoming_msg; ///< which we should reply to when done
		mutable boost::shared_ptr<deadline_timer> deletion_callback; ///< should be canceled when registration succeeded with a heartbeat
	};

	typedef boost::multi_index::multi_index_container<
		engine_connector,
		boost::multi_index::indexed_by<
			// sort by ID
			boost::multi_index::ordered_unique<boost::multi_index::tag<struct engine_id_t>,boost::multi_index::member<engine_connector,int,&engine_connector::id> >,
			// sort by queue name
			boost::multi_index::ordered_non_unique<boost::multi_index::tag<struct queue_name_t>,boost::multi_index::member<engine_connector,std::string,&engine_connector::queue> >,
			// sort by heart name
			boost::multi_index::ordered_non_unique<boost::multi_index::tag<struct heart_name_t>,boost::multi_index::member<engine_connector,std::string,&engine_connector::heartbeat> >
		> 
	> engine_connector_set;
	typedef boost::multi_index::index<engine_connector_set,engine_id_t>::type engine_connector_set_by_id;
	typedef boost::multi_index::index<engine_connector_set,queue_name_t>::type engine_connector_set_by_queue;
	typedef boost::multi_index::index<engine_connector_set,heart_name_t>::type engine_connector_set_by_heart;

	struct engine_tracker{
		engine_connector_set          engines;
		engine_connector_set          incoming_registrations;
		std::set<std::string>         dead_engines;
		int next_id();
	};

}

#endif /* __GPF_ENGINE_SET_HPP__ */
