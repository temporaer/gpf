ADD_LIBRARY(gpf SHARED 
	controller/hub.cpp
	controller/heartmonitor.cpp
	util/zmqmessage.cpp
	util/message_util.cpp
	util/url_handling.cpp
	)
TARGET_LINK_LIBRARIES(gpf zmq glog ${Boost_LIBRARIES})
