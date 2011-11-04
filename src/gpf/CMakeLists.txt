add_subdirectory(messages)
INCLUDE_DIRECTORIES(${CMAKE_BINARY_DIR}/src)

ADD_LIBRARY(gpf SHARED 
	controller/hub.cpp
	controller/heartmonitor.cpp
	client/client.cpp
	engine/engine.cpp
	util/zmqmessage.cpp
	util/url_handling.cpp
	)
TARGET_LINK_LIBRARIES(gpf gpf_messages zmq glog ${Boost_LIBRARIES})
