PROJECT(gpf)
set(PROJECT_NAME_STR gpf)
cmake_minimum_required(VERSION 2.8)
SET(CMAKE_MODULE_PATH  ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/CMakeModules )
include(CMakeForceCompiler)
CMAKE_FORCE_CXX_COMPILER(g++-4.5 GNU)

#add_subdirectory(third_party)
add_subdirectory(src)
