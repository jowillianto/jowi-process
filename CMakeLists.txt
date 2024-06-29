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
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../file-lock file-lock)
  add_executable(test_child ${CMAKE_CURRENT_LIST_DIR}/tests/test_child.cpp)
  moderna_add_test(
    moderna_process_tests
    ${CMAKE_CURRENT_LIST_DIR}/tests/tests.cpp
    LIBRARIES moderna_test_lib moderna_process
    COMPILE_DEFINITIONS TEST_CHILD_EXEC="${CMAKE_CURRENT_BINARY_DIR}/test_child"
    SANITIZERS all
  )
  moderna_add_test(
    moderna_io_tests
    ${CMAKE_CURRENT_LIST_DIR}/tests/io_tests.cpp
    LIBRARIES moderna_test_lib moderna_process moderna_file_lock
    COMPILE_DEFINITIONS 
      READ_FILE="${CMAKE_CURRENT_LIST_DIR}/assets/read.txt" 
      WRITE_FILE="${CMAKE_CURRENT_LIST_DIR}/assets/write.txt"
    SANITIZERS all
  )
endif()