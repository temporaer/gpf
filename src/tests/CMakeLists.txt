find_package(GTest REQUIRED)

CMAKE_FORCE_CXX_COMPILER(g++ GNU)

set(PROJECT_TEST_NAME ${PROJECT_NAME_STR}_test)
include_directories(${GTEST_INCLUDE_DIRS})

file(GLOB TEST_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(${PROJECT_TEST_NAME} ${TEST_SRC_FILES})
target_link_libraries(${PROJECT_TEST_NAME}  ${Boost_LIBRARIES} ${GTEST_BOTH_LIBRARIES} gpf pthread)

add_test(gtest ${PROJECT_TEST_NAME})
