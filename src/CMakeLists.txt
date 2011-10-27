PROJECT(gpf)
set(PROJECT_NAME_STR gpf)
cmake_minimum_required(VERSION 2.8)
SET(CMAKE_MODULE_PATH  ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/CMakeModules )

FIND_PACKAGE(Qt4 REQUIRED)

SET (Boost_FIND_REQUIRED     TRUE)
SET (Boost_FIND_QUIETLY      TRUE)
SET (Boost_USE_MULTITHREADED TRUE)
SET (Boost_USE_STATIC_LIBS   FALSE)
FIND_PACKAGE( Boost 1.37 COMPONENTS date_time serialization program_options iostreams filesystem REQUIRED )
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -Wall -Wno-deprecated -pthread")

ENABLE_TESTING()

include_directories(${CMAKE_SOURCE_DIR})
include_directories(zmq-poll-wrapper)

add_subdirectory(gpf)
add_subdirectory(tests EXCLUDE_FROM_ALL)
