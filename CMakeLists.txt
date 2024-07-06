cmake_minimum_required(VERSION 3.28)
project(moderna_process)
set (CMAKE_CXX_STANDARD 23)
set (CMAKE_CXX_STANDARD_REQUIRED true)
set (CMAKE_CXX_SCAN_FOR_MODULES true)

if (NOT ${PROJECT_NAME})
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libs/variant_error)
  add_library(${PROJECT_NAME})
  file (
    GLOB ${PROJECT_NAME}_src
    "${CMAKE_CURRENT_LIST_DIR}/src/*.ccm"
  )
  target_sources(${PROJECT_NAME}
    PUBLIC 
      FILE_SET CXX_MODULES FILES ${${PROJECT_NAME}_src}
  )
endif()

if (
  EXISTS ${CMAKE_CURRENT_LIST_DIR}/../test-lib
  AND PROJECT_IS_TOP_LEVEL
)
  include(CTest)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../test-lib test-lib)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../file-lock file-lock)
  add_executable(test_child ${CMAKE_CURRENT_LIST_DIR}/tests/test_child.cpp)
  moderna_add_test(
    ${PROJECT_NAME}_tests
    ${CMAKE_CURRENT_LIST_DIR}/tests/tests.cpp
    LIBRARIES moderna_test_lib ${PROJECT_NAME}
    COMPILE_DEFINITIONS TEST_CHILD_EXEC="${CMAKE_CURRENT_BINARY_DIR}/test_child"
    SANITIZERS all
  )
endif()