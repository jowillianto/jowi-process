cmake_minimum_required(VERSION 3.28)
project(moderna_process)
set (CMAKE_CXX_STANDARD 23)
set (CMAKE_CXX_STANDARD_REQUIRED true)
set (CMAKE_CXX_SCAN_FOR_MODULES true)
add_library(moderna_process)
file (
  GLOB moderna_process_src
  "${CMAKE_CURRENT_LIST_DIR}/src/*.ccm"
)
target_sources(moderna_process
  PUBLIC 
    FILE_SET CXX_MODULES FILES ${moderna_process_src}
)

if (
  EXISTS ${CMAKE_CURRENT_LIST_DIR}/../test-lib
  AND PROJECT_IS_TOP_LEVEL
)
  include(CTest)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../test-lib test-lib)
  add_executable(test_child ${CMAKE_CURRENT_LIST_DIR}/tests/test_child.cpp)
  moderna_add_test(
    moderna_process_tests
    ${CMAKE_CURRENT_LIST_DIR}/tests/tests.cpp
    LIBRARIES moderna_test_lib moderna_process
    COMPILE_DEFINITIONS TEST_CHILD_EXEC="${CMAKE_CURRENT_BINARY_DIR}/test_child"
    SANITIZERS all
  )
endif()