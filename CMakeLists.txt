cmake_minimum_required(VERSION 3.28)
project(jowi_process VERSION 1.0.0)
set (CMAKE_CXX_STANDARD 23)
set (CMAKE_CXX_STANDARD_REQUIRED true)
set (CMAKE_CXX_SCAN_FOR_MODULES true)

option (JOWI_PROCESS_BUILD_TESTS "Build tests" OFF)
option (JOWI_PROCESS_INTEGRATE_IO "Enable integration with jowi.io helpers" OFF)
option (JOWI_PROCESS_INTEGRATE_ASIO "Enable integration with jowi.asio helpers" OFF)

if (NOT TARGET jowi::generic)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libs/jowi-generic)
endif()

if (JOWI_PROCESS_INTEGRATE_ASIO AND NOT TARGET jowi::asio)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libs/jowi-asio)
endif()

if (JOWI_PROCESS_INTEGRATE_IO AND NOT TARGET jowi::io)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libs/jowi-io)
endif()


add_library(${PROJECT_NAME})
add_library(jowi::process ALIAS ${PROJECT_NAME})
set(JOWI_PROCESS_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/src/main.cc
    ${CMAKE_CURRENT_LIST_DIR}/src/subprocess_argument.cc
    ${CMAKE_CURRENT_LIST_DIR}/src/subprocess_env.cc
    ${CMAKE_CURRENT_LIST_DIR}/src/subprocess_error.cc
)
if (JOWI_PROCESS_INTEGRATE_ASIO)
    list(APPEND JOWI_PROCESS_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/src/asio.cc
    )
endif()

if (WIN32)
    list(APPEND JOWI_PROCESS_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/src/platform/windows/subprocess.cc
        ${CMAKE_CURRENT_LIST_DIR}/src/platform/windows/subprocess_result.cc
        ${CMAKE_CURRENT_LIST_DIR}/src/platform/windows/unique_pid.cc
    )
else()
    list(APPEND JOWI_PROCESS_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/src/platform/unix/subprocess.cc
        ${CMAKE_CURRENT_LIST_DIR}/src/platform/unix/subprocess_result.cc
        ${CMAKE_CURRENT_LIST_DIR}/src/platform/unix/unique_pid.cc
    )
endif()

target_sources(${PROJECT_NAME}
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
      ${JOWI_PROCESS_SOURCES}
)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
target_link_libraries(${PROJECT_NAME}
  PUBLIC
    jowi::generic
)

if (JOWI_PROCESS_INTEGRATE_IO)
    target_link_libraries(${PROJECT_NAME} PUBLIC jowi::io)
    target_compile_definitions(${PROJECT_NAME}
        PUBLIC
            JOWI_PROCESS_INTEGRATE_IO=""
    )
endif()
if (JOWI_PROCESS_INTEGRATE_ASIO)
    target_link_libraries(${PROJECT_NAME} PUBLIC jowi::asio)
    target_compile_definitions(${PROJECT_NAME}
        PUBLIC
            JOWI_PROCESS_INTEGRATE_ASIO=""
    )
endif()

if (JOWI_PROCESS_BUILD_TESTS)
    if (NOT JOWI_PROCESS_INTEGRATE_IO OR NOT JOWI_PROCESS_INTEGRATE_ASIO)
        message(WARNING "Tests require IO and ASIO integrations enabled; skipping test build.")
    else()
        include(CTest)
        if (NOT TARGET jowi::test_lib)
            add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libs/test-lib)
        endif()
        add_executable(process_child ${CMAKE_CURRENT_LIST_DIR}/tests/test_child.cpp)
        target_link_libraries(process_child PRIVATE jowi::process)
        jowi_add_test(test_jowi_process_basic
            ${CMAKE_CURRENT_LIST_DIR}/tests/tests.cpp
            LIBRARIES ${PROJECT_NAME} jowi::io
            COMPILE_DEFINITIONS TEST_CHILD_EXEC="${CMAKE_CURRENT_BINARY_DIR}/process_child"
            SANITIZERS all
        )
    endif()
endif()

if (JOWI_INSTALL)
    include(GNUInstallDirs)
    set (JOWI_COMPONENT_NAME "process")
    set_property(
    TARGET ${PROJECT_NAME}
    PROPERTY EXPORT_NAME ${JOWI_COMPONENT_NAME}
  )
    install (
    TARGETS ${PROJECT_NAME}
    EXPORT jowi
    FILE_SET
      CXX_MODULES
      DESTINATION ${CMAKE_INSTALL_PREFIX}/cxx_modules/jowi/${JOWI_COMPONENT_NAME}
      COMPONENT ${JOWI_COMPONENT_NAME}_file_set
    CXX_MODULES_BMI
      COMPONENT ${JOWI_COMPONENT_NAME}
      DESTINATION ${CMAKE_INSTALL_PREFIX}/cxx_modules/jowi/${JOWI_COMPONENT_NAME}
  )
    if (NOT JOWI_GLOBAL_INSTALL)
        install (
      EXPORT jowi
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/jowi
      FILE jowi-config.cmake
      NAMESPACE jowi::
      CXX_MODULES_DIRECTORY cxx_modules/jowi
    )
    endif()
endif()
