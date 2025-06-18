cmake_minimum_required(VERSION 3.28)
project(moderna_process)
set (CMAKE_CXX_STANDARD 23)
set (CMAKE_CXX_STANDARD_REQUIRED true)
set (CMAKE_CXX_SCAN_FOR_MODULES true)
set (MODERNA_COMPONENT_NAME "file_lock")

option (MODERNA_PROCESS_BUILD_TESTS "Build tests" OFF)

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
  target_link_libraries(${PROJECT_NAME} PUBLIC moderna_io)
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

include(GNUInstallDirs)
install (
  TARGETS ${PROJECT_NAME}
  EXPORT moderna
  FILE_SET 
    CXX_MODULES 
    DESTINATION ${CMAKE_INSTALL_PREFIX}/cxx_modules/moderna/${MODERNA_COMPONENT_NAME}
    COMPONENT ${MODERNA_COMPONENT_NAME}_file_set
  CXX_MODULES_BMI 
    COMPONENT ${MODERNA_COMPONENT_NAME}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/cxx_modules/moderna/${MODERNA_COMPONENT_NAME}
)

if (PROJECT_IS_TOP_LEVEL)
  install (
    EXPORT moderna
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/moderna
    FILE moderna-config.cmake
    NAMESPACE moderna::
    CXX_MODULES_DIRECTORY cxx_modules/moderna
  )
endif()