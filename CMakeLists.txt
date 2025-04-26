cmake_minimum_required(VERSION 3.28)
project(moderna_process)
set (CMAKE_CXX_STANDARD 23)
set (CMAKE_CXX_STANDARD_REQUIRED true)
set (CMAKE_CXX_SCAN_FOR_MODULES true)

option (MODERNA_PROCESS_COMMAND_FORWARDER "Build the command forwarder" OFF)

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

if (MODERNA_PROCESS_COMMAND_FORWARDER)
  add_executable(moderna_command_forwarder ${CMAKE_CURRENT_LIST_DIR}/main.cc)
  target_link_libraries(moderna_command_forwarder PUBLIC moderna_process)
endif()