PROJECT(gpf)
cmake_minimum_required(VERSION 2.8)
FIND_PACKAGE(Qt4 REQUIRED)

include_directories(zmq-poll-wrapper)
add_subdirectory(gpf)
