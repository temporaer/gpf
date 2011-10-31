SET (Boost_FIND_REQUIRED     TRUE)
SET (Boost_FIND_QUIETLY      TRUE)
SET (Boost_USE_MULTITHREADED TRUE)
SET (Boost_USE_STATIC_LIBS   FALSE)
FIND_PACKAGE( Boost 1.37 COMPONENTS date_time serialization program_options iostreams filesystem thread REQUIRED )
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -Wall -Wno-deprecated -pthread")

ENABLE_TESTING()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/zmqmessage/include)
include_directories(zmq-poll-wrapper)

add_subdirectory(gpf)
add_subdirectory(tests EXCLUDE_FROM_ALL)
