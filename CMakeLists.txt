cmake_minimum_required(VERSION 3.28)
project(moderna_process)
set (CMAKE_CXX_STANDARD 23)
set (CMAKE_CXX_STANDARD_REQUIRED true)
set (CMAKE_CXX_SCAN_FOR_MODULES true)

option (MODERNA_PROCESS_BUILD_TESTS "Build tests" OFF)

add_library(${PROJECT_NAME})
add_library(moderna::process ALIAS ${PROJECT_NAME})
file (
  GLOB ${PROJECT_NAME}_src
  "${CMAKE_CURRENT_LIST_DIR}/src/*.cc"
)
target_sources(${PROJECT_NAME}
  PUBLIC 
    FILE_SET CXX_MODULES FILES ${${PROJECT_NAME}_src}
)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

if (MODERNA_PROCESS_BUILD_TESTS)
  if (NOT TARGET moderna_test_lib)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libs/test-lib)
  endif()
  add_executable(process_child ${CMAKE_CURRENT_LIST_DIR}/tests/test_child.cpp)
  moderna_add_test(process_tests 
    ${CMAKE_CURRENT_LIST_DIR}/tests/tests.cpp
    LIBRARIES ${PROJECT_NAME} moderna::io
    COMPILE_DEFINITIONS TEST_CHILD_EXEC="${CMAKE_CURRENT_BINARY_DIR}/process_child"
    SANITIZERS all
  )
endif()

if (MODERNA_INSTALL)
  include(GNUInstallDirs)
  set (MODERNA_COMPONENT_NAME "process")
  set_property(
    TARGET ${PROJECT_NAME} 
    PROPERTY EXPORT_NAME ${MODERNA_COMPONENT_NAME}
  )
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
  if (NOT MODERNA_GLOBAL_INSTALL)
    install (
      EXPORT moderna
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/moderna
      FILE moderna-config.cmake
      NAMESPACE moderna::
      CXX_MODULES_DIRECTORY cxx_modules/moderna
    )
  endif()
endif()