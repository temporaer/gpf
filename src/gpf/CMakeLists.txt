ADD_LIBRARY(gpf SHARED controller/hub.cpp)
TARGET_LINK_LIBRARIES(gpf zmq glog)
