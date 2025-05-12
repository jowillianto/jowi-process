cmake_minimum_required(VERSION 3.28)
project(moderna_process)
set (CMAKE_CXX_STANDARD 23)
set (CMAKE_CXX_STANDARD_REQUIRED true)
set (CMAKE_CXX_SCAN_FOR_MODULES true)

option (MODERNA_PROCESS_BUILD_TESTS "Build tests" OFF)

if (NOT TARGET moderna_io) 
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libs/moderna-io)
endif()

if (NOT TARGET ${PROJECT_NAME})
  add_library(${PROJECT_NAME})
  file (
    GLOB ${PROJECT_NAME}_src
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cc"
  )
  target_sources(${PROJECT_NAME}
    PUBLIC 
      FILE_SET CXX_MODULES FILES ${${PROJECT_NAME}_src}
  )
endif()

if (MODERNA_PROCESS_BUILD_TESTS)
  if (NOT TARGET moderna_test_lib)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libs/test-lib)
  endif()
  add_executable(process_child ${CMAKE_CURRENT_LIST_DIR}/tests/test_child.cpp)
  moderna_add_test(process_tests 
    ${CMAKE_CURRENT_LIST_DIR}/tests/tests.cpp
    LIBRARIES ${PROJECT_NAME} moderna_io
    COMPILE_DEFINITIONS TEST_CHILD_EXEC="${CMAKE_CURRENT_BINARY_DIR}/process_child"
    SANITIZERS all
  )
endif()