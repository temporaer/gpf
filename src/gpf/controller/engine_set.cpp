#include <zmq-poll-wrapper/reactor.hpp>
#include <gpf/controller/engine_set.hpp>

namespace gpf
{
	int engine_tracker::next_id(){
			static int id = 0;
			return id++;
	}
}
