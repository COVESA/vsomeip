# Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required (VERSION 2.8.12...3.23.3)
project (vsomeip)

set (VSOMEIP_NAME vsomeip3)
set (VSOMEIP_COMPAT_NAME vsomeip)

set (VSOMEIP_MAJOR_VERSION 3)
set (VSOMEIP_MINOR_VERSION 1)
set (VSOMEIP_PATCH_VERSION 20)
set (VSOMEIP_HOTFIX_VERSION 3)

set (VSOMEIP_VERSION ${VSOMEIP_MAJOR_VERSION}.${VSOMEIP_MINOR_VERSION}.${VSOMEIP_PATCH_VERSION})
set (PACKAGE_VERSION ${VSOMEIP_VERSION}) # Used in documentation/doxygen.in
set (CMAKE_VERBOSE_MAKEFILE off)

if (NOT GTEST_ROOT)
    if (DEFINED ENV{GTEST_ROOT})
        set(GTEST_ROOT $ENV{GTEST_ROOT})
    else()
        set(GTEST_ROOT "n/a" CACHE STRING "Path to root folder of googletest. Must be set for building the tests.")
    endif()
endif()

###################################################################################################
# see http://www.cmake.org/Wiki/CMake/Tutorials/How_to_create_a_ProjectConfig.cmake_file
###################################################################################################

# Offer the user the choice of overriding the installation directories
set (INSTALL_LIB_DIR lib CACHE STRING "Installation directory for libraries")
set (INSTALL_BIN_DIR bin CACHE STRING "Installation directory for executables")
set (INSTALL_INCLUDE_DIR include CACHE STRING "Installation directory for header files")

if (WIN32 AND NOT CYGWIN)
    set (DEF_INSTALL_CMAKE_DIR CMake)
else ()
    set (DEF_INSTALL_CMAKE_DIR lib/cmake/${VSOMEIP_NAME})
endif ()

set (INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE STRING "Installation directory for CMake files")

###################################################################################################
# Set a default build type if none was specified
set(default_build_type "RelWithDebInfo")
if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# OS
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(OS "LINUX")
    set(DL_LIBRARY "dl")
    set(EXPORTSYMBOLS "-Wl,-export-dynamic -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exportmap.gcc")
    set(NO_DEPRECATED "")
    set(OPTIMIZE "")
    set(OS_CXX_FLAGS "-D_GLIBCXX_USE_NANOSLEEP -pthread -O -Wall -Wextra -Wformat -Wformat-security -Wconversion -fexceptions -fstrict-aliasing -fstack-protector-strong -fasynchronous-unwind-tables -fno-omit-frame-pointer -D_FORTIFY_SOURCE=2 -Wformat -Wformat-security -Werror=format-security -fPIE -pie -Wl,-z,relro,-z,now")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

if (${CMAKE_SYSTEM_NAME} MATCHES "Android")
    set(OS "ANDROID")
    set(DL_LIBRARY "")
    set(EXPORTSYMBOLS "")
    set(NO_DEPRECATED "")
    set(OPTIMIZE "")

    find_library(ANDROID_LOG_LIB log)
    set(OS_LIBS ${ANDROID_LOG_LIB})
endif(${CMAKE_SYSTEM_NAME} MATCHES "Android")

if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    set(OS "FREEBSD")
    set(DL_LIBRARY "")
    set(EXPORTSYMBOLS "")
    set(NO_DEPRECATED "-Wno-deprecated")
    set(OPTIMIZE "")
    set(OS_CXX_FLAGS "-pthread")
endif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")

################################################################################
# Options
################################################################################

# DLT
if (DISABLE_DLT)
set (VSOMEIP_ENABLE_DLT 0)
else ()
set (VSOMEIP_ENABLE_DLT 1)
endif ()

# Signal handling
if (ENABLE_SIGNAL_HANDLING)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVSOMEIP_ENABLE_SIGNAL_HANDLING")
endif ()

# Sanitizer
if (ENABLE_THREAD_SANITIZER)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
endif ()

if (ENABLE_LEAK_SANITIZER)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=leak")
endif ()

# Configuration overlays
if (ENABLE_CONFIGURATION_OVERLAYS)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVSOMEIP_ENABLE_CONFIGURATION_OVERLAYS")
endif ()

# Compatibility
if (ENABLE_COMPAT)
set (VSOMEIP_ENABLE_COMPAT 1)
else ()
set (VSOMEIP_ENABLE_COMPAT 0)
endif ()

# Multiple routing managers
if (ENABLE_MULTIPLE_ROUTING_MANAGERS)
set (VSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS 1)
else ()
set (VSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS 0)
endif ()

# Session handling configuration
if (ENABLE_SESSION_HANDLING_CONFIG)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVSOMEIP_HAS_SESSION_HANDLING_CONFIG")
endif ()

################################################################################
# Dependencies
################################################################################

# Threads
find_package(Threads REQUIRED)

# Boost
find_package( Boost 1.55 COMPONENTS system thread filesystem REQUIRED )
include_directories( ${Boost_INCLUDE_DIR} )

if(Boost_FOUND)
  if(Boost_LIBRARY_DIR)
    MESSAGE( STATUS "Boost_LIBRARY_DIR not empty using it: ${Boost_LIBRARY_DIR}" )
  else()
    if(BOOST_LIBRARYDIR)
      MESSAGE( STATUS "Boost_LIBRARY_DIR empty but BOOST_LIBRARYDIR is set setting Boost_LIBRARY_DIR to: ${BOOST_LIBRARYDIR}" )
      set(Boost_LIBRARY_DIR ${BOOST_LIBRARYDIR})
    endif()
  endif()
else()
  MESSAGE( STATUS "Boost was not found!")
endif()

# cmake 3.15 introduced a new variable and a new format for the old one
if (DEFINED Boost_VERSION_MACRO)
  set(VSOMEIP_BOOST_VERSION ${Boost_VERSION_MACRO})
else()
  set(VSOMEIP_BOOST_VERSION ${Boost_VERSION})
endif()

message( STATUS "Using boost version: ${VSOMEIP_BOOST_VERSION}" )
if (${VSOMEIP_BOOST_VERSION} GREATER 107600)
message( ERROR "boost version ${VSOMEIP_BOOST_VERSION} is not (yet) supported. Latest supported version is 1.76.0" )
elseif(${VSOMEIP_BOOST_VERSION} GREATER 107500)
set(VSOMEIP_BOOST_HELPER implementation/helper/1.76)
elseif(${VSOMEIP_BOOST_VERSION} GREATER 107300)
set(VSOMEIP_BOOST_HELPER implementation/helper/1.74)
elseif(${VSOMEIP_BOOST_VERSION} GREATER 106999)
set(VSOMEIP_BOOST_HELPER implementation/helper/1.70)
elseif(${VSOMEIP_BOOST_VERSION} GREATER 106599)
set(VSOMEIP_BOOST_HELPER implementation/helper/1.66)
else()
set(VSOMEIP_BOOST_HELPER implementation/helper/1.55)
endif()

find_package(PkgConfig)

# DLT
if(VSOMEIP_ENABLE_DLT EQUAL 1)
pkg_check_modules(DLT "automotive-dlt >= 2.11")
if(DLT_FOUND) 
     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_DLT")
endif(DLT_FOUND)
endif()

# SystemD
pkg_check_modules(SystemD "libsystemd")

if(NOT SystemD_FOUND OR ${CMAKE_SYSTEM_NAME} MATCHES "Android")
MESSAGE( STATUS "Systemd was not found, watchdog disabled!")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWITHOUT_SYSTEMD")
else()
list(APPEND OS_LIBS ${SystemD_LIBRARIES})
endif(NOT SystemD_FOUND OR ${CMAKE_SYSTEM_NAME} MATCHES "Android")

# Multiple routing managers
if (VSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS EQUAL 1)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS")
endif ()

################################################################################
# Directories
################################################################################

include_directories(
    interface
)

include_directories(SYSTEM
    ${VSOMEIP_BOOST_HELPER}
    ${DLT_INCLUDE_DIRS}
)

link_directories(
    ${DLT_LIBDIR}
)

if (${VSOMEIP_HOTFIX_VERSION} EQUAL 0)
add_definitions(-DVSOMEIP_VERSION="${VSOMEIP_VERSION}")
else()
add_definitions(-DVSOMEIP_VERSION="${VSOMEIP_VERSION}.${VSOMEIP_HOTFIX_VERSION}")
endif()

if (MSVC)
    message("using MSVC Compiler")
    # add_definitions(-DVSOMEIP_DLL_COMPILATION) now it is controlled per target
    SET(BOOST_WINDOWS_VERSION "0x600" CACHE STRING "Set the same Version as the Version with which Boost was built, otherwise there will be errors. (normaly 0x600 is for Windows 7 and 0x501 is for Windows XP)")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_WINSOCK_DEPRECATED_NO_WARNINGS -D_WIN32_WINNT=${BOOST_WINDOWS_VERSION} -DWIN32 -DBOOST_ASIO_DISABLE_IOCP /EHsc")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS -D_WIN32_WINNT=${BOOST_WINDOWS_VERSION} -DWIN32 -DBOOST_ASIO_DISABLE_IOCP /EHsc")
    set(USE_RT "")
    link_directories(${Boost_LIBRARY_DIR_DEBUG})
    ADD_DEFINITIONS( -DBOOST_ALL_DYN_LINK )
else()
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D${OS} ${OS_CXX_FLAGS} -g ${OPTIMIZE} -std=c++11 ${NO_DEPRECATED} ${EXPORTSYMBOLS}")
    if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Android")
        set(USE_RT "rt")
    endif()
endif()

################################################################################
# Configuration library
################################################################################
file(GLOB ${VSOMEIP_NAME}-cfg_SRC
    "implementation/configuration/src/*.cpp"
)
list(SORT ${VSOMEIP_NAME}-cfg_SRC)
if (VSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS EQUAL 0)
    add_library(${VSOMEIP_NAME}-cfg SHARED ${${VSOMEIP_NAME}-cfg_SRC})
    set_target_properties (${VSOMEIP_NAME}-cfg PROPERTIES VERSION ${VSOMEIP_VERSION} SOVERSION ${VSOMEIP_MAJOR_VERSION})
    if (MSVC)
        set_target_properties(${VSOMEIP_NAME}-cfg PROPERTIES COMPILE_DEFINITIONS "VSOMEIP_DLL_COMPILATION_PLUGIN")
    endif()

    target_link_libraries(${VSOMEIP_NAME}-cfg ${VSOMEIP_NAME} ${Boost_LIBRARIES} ${USE_RT} ${DL_LIBRARY} ${OS_LIBS})
endif ()

################################################################################
# Base library
################################################################################
file(GLOB ${VSOMEIP_NAME}_SRC
    "implementation/endpoints/src/*.cpp"
    "implementation/logger/src/*.cpp"
    "implementation/tracing/src/*.cpp"
    "implementation/message/src/*.cpp"
    "implementation/plugin/src/*.cpp"
    "implementation/routing/src/*.cpp"
    "implementation/runtime/src/*.cpp"
    "implementation/security/src/*.cpp"
    "implementation/utility/src/*.cpp"
)
if (VSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS EQUAL 1)
list(APPEND ${VSOMEIP_NAME}_SRC "implementation/configuration/src/configuration_impl.cpp")
endif()
list(SORT ${VSOMEIP_NAME}_SRC)

add_library(${VSOMEIP_NAME} SHARED ${${VSOMEIP_NAME}_SRC})
set_target_properties (${VSOMEIP_NAME} PROPERTIES VERSION ${VSOMEIP_VERSION} SOVERSION ${VSOMEIP_MAJOR_VERSION})
if (MSVC)
    set_target_properties(${VSOMEIP_NAME} PROPERTIES COMPILE_DEFINITIONS "VSOMEIP_DLL_COMPILATION")
endif ()
target_include_directories(${VSOMEIP_NAME} INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/interface>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
    $<INSTALL_INTERFACE:${INSTALL_INCLUDE_DIR}>)
# PRIVATE means the listed libraries won't be included in the "link interface",
# meaning the exported vsomeip3Targets.cmake targets won't try to link against
# them (which shouldn't be required). ${Boost_LIBRARIES} includes absolute
# build host paths as of writing, which also makes this important as it breaks
# the build.
target_link_libraries(${VSOMEIP_NAME} PRIVATE ${Boost_LIBRARIES} ${USE_RT} ${DL_LIBRARY} ${DLT_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${OS_LIBS})

################################################################################
# Service Discovery library
################################################################################
file(GLOB ${VSOMEIP_NAME}-sd_SRC
    "implementation/service_discovery/src/*.cpp"
)
list(SORT ${VSOMEIP_NAME}-sd_SRC)

add_library(${VSOMEIP_NAME}-sd SHARED ${${VSOMEIP_NAME}-sd_SRC})
set_target_properties (${VSOMEIP_NAME}-sd PROPERTIES VERSION ${VSOMEIP_VERSION} SOVERSION ${VSOMEIP_MAJOR_VERSION})
if (MSVC)
    set_target_properties(${VSOMEIP_NAME}-sd PROPERTIES COMPILE_DEFINITIONS "VSOMEIP_DLL_COMPILATION_PLUGIN")
endif ()

target_link_libraries(${VSOMEIP_NAME}-sd ${VSOMEIP_NAME} ${Boost_LIBRARIES} ${USE_RT} ${DL_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${OS_LIBS})


################################################################################
# E2E library
################################################################################
file(GLOB_RECURSE ${VSOMEIP_NAME}-e2e_SRC
    "implementation/e2e_protection/src/*.cpp"
)
list(SORT ${VSOMEIP_NAME}-e2e_SRC)

add_library(${VSOMEIP_NAME}-e2e SHARED ${${VSOMEIP_NAME}-e2e_SRC})
set_target_properties (${VSOMEIP_NAME}-e2e PROPERTIES VERSION ${VSOMEIP_VERSION} SOVERSION ${VSOMEIP_MAJOR_VERSION})
if (MSVC)
    set_target_properties(${VSOMEIP_NAME}-e2e PROPERTIES COMPILE_DEFINITIONS "VSOMEIP_DLL_COMPILATION_PLUGIN")
endif ()

target_link_libraries(${VSOMEIP_NAME}-e2e ${VSOMEIP_NAME} ${Boost_LIBRARIES} ${USE_RT} ${DL_LIBRARY} ${OS_LIBS})

################################################################################
# Compatibility library
################################################################################
if (VSOMEIP_ENABLE_COMPAT EQUAL 1)
set (VSOMEIP_COMPAT_MAJOR_VERSION 2)
set (VSOMEIP_COMPAT_VERSION ${VSOMEIP_COMPAT_MAJOR_VERSION}.99.99)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVSOMEIP_ENABLE_COMPAT")

file(GLOB_RECURSE ${VSOMEIP_COMPAT_NAME}_SRC
    "implementation/compat/logging/src/*.cpp"
    "implementation/compat/message/src/*.cpp"
    "implementation/compat/runtime/src/*.cpp"
)
list(SORT ${VSOMEIP_COMPAT_NAME}_SRC)

add_library(${VSOMEIP_COMPAT_NAME} SHARED ${${VSOMEIP_COMPAT_NAME}_SRC})
set_target_properties (${VSOMEIP_COMPAT_NAME} PROPERTIES VERSION ${VSOMEIP_COMPAT_VERSION} SOVERSION ${VSOMEIP_COMPAT_MAJOR_VERSION})
if (MSVC)
    set_target_properties(${VSOMEIP_COMPAT_NAME} PROPERTIES COMPILE_DEFINITIONS "VSOMEIP_DLL_COMPILATION_PLUGIN")
endif ()

target_include_directories(
    ${VSOMEIP_COMPAT_NAME}
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}> # for headers when building
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for generated files in build mode
    $<INSTALL_INTERFACE:include/compat> # for clients in install mode
)
target_link_libraries(${VSOMEIP_COMPAT_NAME} PRIVATE ${VSOMEIP_NAME} ${Boost_LIBRARIES} ${USE_RT} ${DL_LIBRARY} ${OS_LIBS})

endif ()

################################################################################
# Configuration files
################################################################################
set(EXAMPLE_CONFIG_FILES
    "config/vsomeip.json"
    "config/vsomeip-local.json"
    "config/vsomeip-tcp-client.json"
    "config/vsomeip-tcp-service.json"
    "config/vsomeip-udp-client.json"
    "config/vsomeip-udp-service.json"
)

################################################################################
# Configuration parameters
################################################################################
set (VSOMEIP_BASE_PATH "/tmp")
if (BASE_PATH)
set (VSOMEIP_BASE_PATH ${BASE_PATH})
endif ()
set (VSOMEIP_DIAGNOSIS_ADDRESS "0x01")
if (DIAGNOSIS_ADDRESS)
set (VSOMEIP_DIAGNOSIS_ADDRESS ${DIAGNOSIS_ADDRESS})
endif ()
set (VSOMEIP_UNICAST_ADDRESS "127.0.0.1")
if (UNICAST_ADDRESS)
set (VSOMEIP_UNICAST_ADDRESS ${UNICAST_ADDRESS})
endif ()
set (VSOMEIP_ROUTING_READY_MESSAGE "SOME/IP routing ready.")
if (ROUTING_READY_MESSAGE)
set (VSOMEIP_ROUTING_READY_MESSAGE ${ROUTING_READY_MESSAGE})
endif ()

set(DEFAULT_CONFIGURATION_FOLDER "/etc/vsomeip" CACHE PATH "Default configuration folder")
message(STATUS "Default configuration folder: ${DEFAULT_CONFIGURATION_FOLDER}")

set(DEFAULT_CONFIGURATION_FILE "/etc/vsomeip.json" CACHE FILEPATH "Default configuration file")
message(STATUS "Default configuration file: ${DEFAULT_CONFIGURATION_FILE}")

message("Predefined base path: ${VSOMEIP_BASE_PATH}")
message("Predefined unicast address: ${VSOMEIP_UNICAST_ADDRESS}")
message("Predefined diagnosis address: ${VSOMEIP_DIAGNOSIS_ADDRESS}")

################################################################################
# Installation
################################################################################
set(INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/interface/vsomeip")

file (GLOB_RECURSE vsomeip_INCLUDE RELATIVE ${INCLUDE_PATH} "interface/*.hpp"  )
list (SORT vsomeip_INCLUDE)

foreach ( file ${vsomeip_INCLUDE} )
    get_filename_component( dir ${file} DIRECTORY )
    install( FILES "${INCLUDE_PATH}/${file}" DESTINATION "${INSTALL_INCLUDE_DIR}/vsomeip/${dir}" COMPONENT dev)
endforeach()

install (
    TARGETS ${VSOMEIP_NAME} 
    # IMPORTANT: Add the vsomeip library to the "export-set"
    EXPORT vsomeip3Targets
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT
    ARCHIVE DESTINATION "${INSTALL_LIB_DIR}"
    COMPONENT dev
)

install (
    TARGETS ${VSOMEIP_NAME}-e2e
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
)

if (VSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS EQUAL 0)
install (
    TARGETS ${VSOMEIP_NAME}-cfg
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
)
endif ()

install (
    TARGETS ${VSOMEIP_NAME}-sd
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
)

if (VSOMEIP_ENABLE_COMPAT EQUAL 1)
install (
    TARGETS ${VSOMEIP_COMPAT_NAME}
    EXPORT vsomeipTargets
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
)

export (TARGETS ${VSOMEIP_COMPAT_NAME} FILE "${PROJECT_BINARY_DIR}/vsomeipTargets.cmake")
export (PACKAGE ${VSOMEIP_COMPAT_NAME})

configure_file (vsomeipConfig.cmake.in "${PROJECT_BINARY_DIR}/vsomeipConfig.cmake" @ONLY)
configure_file (vsomeipConfigVersion.cmake.in "${PROJECT_BINARY_DIR}/vsomeipConfigVersion.cmake" @ONLY)

set (COMPAT_INSTALL_CMAKE_DIR "lib/cmake/${VSOMEIP_COMPAT_NAME}")

install (
    EXPORT vsomeipTargets
    DESTINATION "${COMPAT_INSTALL_CMAKE_DIR}"
    COMPONENT dev
)

install (
    FILES
    "${PROJECT_BINARY_DIR}/vsomeipConfig.cmake"
    "${PROJECT_BINARY_DIR}/vsomeipConfigVersion.cmake"
    DESTINATION "${COMPAT_INSTALL_CMAKE_DIR}"
    COMPONENT dev
)

configure_file(vsomeip.pc.in ${PROJECT_BINARY_DIR}/vsomeip.pc @ONLY)
install(FILES ${PROJECT_BINARY_DIR}/vsomeip.pc DESTINATION lib/pkgconfig)

endif ()

install (
  FILES ${EXAMPLE_CONFIG_FILES} DESTINATION etc/vsomeip COMPONENT config
)

# Add all targets to the build-tree export set
export (TARGETS ${VSOMEIP_NAME} FILE "${PROJECT_BINARY_DIR}/vsomeip3Targets.cmake")

# Export the package for use from the build-tree
# (this registers the build-tree with a global CMake-registry)
export (PACKAGE ${VSOMEIP_NAME})

# Create the vsomeip3Config.cmake and vsomeip3ConfigVersion files
configure_file (vsomeip3Config.cmake.in "${PROJECT_BINARY_DIR}/vsomeip3Config.cmake" @ONLY)
configure_file (vsomeip3ConfigVersion.cmake.in "${PROJECT_BINARY_DIR}/vsomeip3ConfigVersion.cmake" @ONLY)

# configure internal.hpp for correct version number
configure_file (
   "${PROJECT_SOURCE_DIR}/implementation/configuration/include/internal.hpp.in"
   "${PROJECT_SOURCE_DIR}/implementation/configuration/include/internal.hpp"
)

# Install the vsomeip3Config.cmake and vsomeip3ConfigVersion.cmake
install (
    FILES
    "${PROJECT_BINARY_DIR}/vsomeip3Config.cmake"
    "${PROJECT_BINARY_DIR}/vsomeip3ConfigVersion.cmake"
    DESTINATION "${INSTALL_CMAKE_DIR}"
    COMPONENT dev
)

# Install the export set for use with the install-tree
install (
    EXPORT vsomeip3Targets
    DESTINATION "${INSTALL_CMAKE_DIR}"
    COMPONENT dev
)

##############################################################################
# build documentation
##############################################################################
add_custom_target(doc)

find_package(Doxygen)
if (NOT DOXYGEN_FOUND)
    message(WARNING "Doxygen is not installed. Documentation can not be built.")
else()
    # set configuration variables for doxygen.in
    set(PROJECT "vsomeip")
    set(DOCDIR documentation)
    set(SRCDIR .)
    set(GENERATE_HTML YES)
    set(GENERATE_HTMLHELP NO)
    set(GENERATE_CHI NO)
    set(GENERATE_LATEX NO)
    set(GENERATE_PDF NO)
    set(GENERATE_RTF NO)
    set(GENERATE_MAN NO)
    set(GENERATE_XML NO)
    set(HAVE_DOT YES)

    if(HAVE_DOT)
        # Note: the @DOT_PATH@ variable won't be used in doxygen.in as doxygen
        # somehow manages to strip the last slash from the path and therfore no
        # graphs are generated. Therefore dot should be available in your $PATH
        FIND_PROGRAM(DOT_PATH dot)
        if ("${DOT_PATH}" STREQUAL "DOT_PATH-NOTFOUND")
            message(WARNING "dot (graphviz) is not installed. Graphs in documentation can't be generated.")
        else()
            message("dot found")
        endif()
    endif()

    configure_file(documentation/doxygen.in ${PROJECT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doxygen-doc
                      COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/Doxyfile
                      SOURCES ${PROJECT_BINARY_DIR}/Doxyfile)

    add_dependencies(doc doxygen-doc)
endif()

find_program(ASCIIDOC_PATH asciidoc)
find_program(SOURCE_HIGHLIGHT_PATH source-highlight)
if ("${ASCIIDOC_PATH}" STREQUAL "ASCIIDOC_PATH-NOTFOUND")
    message(WARNING "asciidoc is not installed. Readme can not be built.")
elseif("${SOURCE_HIGHLIGHT_PATH}" STREQUAL "SOURCE_HIGHLIGHT_PATH-NOTFOUND")
    message(WARNING "source-highlight is not installed. Readme can not be built.")
else()
    message("asciidoc found")
    message("source-highlight found")
    add_custom_command(TARGET doc
                       POST_BUILD
                       COMMAND asciidoc
                               -a version=${VSOMEIP_VERSION}
                               -b html
                               -o documentation/vsomeipUserGuide.html
                               ${PROJECT_SOURCE_DIR}/documentation/vsomeipUserGuide)
endif()

##############################################################################
# create pkg-config file
if(NOT WIN32)
        configure_file(vsomeip3.pc.in ${PROJECT_BINARY_DIR}/vsomeip3.pc @ONLY)
        install(FILES ${PROJECT_BINARY_DIR}/vsomeip3.pc DESTINATION lib/pkgconfig)
endif()

##############################################################################
if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Android")
    # build routing manager daemon (Non-Windows only)
    if (NOT MSVC)
    add_subdirectory( examples/routingmanagerd )
    endif()

    # build tools
    add_custom_target( tools )
    add_subdirectory( tools )

    # build examples
    add_custom_target( examples )
    add_subdirectory( examples EXCLUDE_FROM_ALL )
    add_custom_target( hello_world )
    add_subdirectory( examples/hello_world  EXCLUDE_FROM_ALL )
endif()

##############################################################################
# Test section
##############################################################################

##############################################################################
# google test

# check for set environment variable
if(${GTEST_ROOT} STREQUAL "n/a")
    message(STATUS "GTEST_ROOT is not defined. For building the tests the variable
             GTEST_ROOT has to be defined. Tests can not be built.")
    # early exit
    return()  # test can not be build -> make commands build_tests and check are not available
else()
    message(STATUS "GTEST_ROOT is set. gtest root path set to ${GTEST_ROOT}")
endif()

# build google test as static library (always) -> therefore deactivate BUILD_SHARED_LIBS in case it is active
set(BUILD_SHARED_LIBS_AUTOMATIC_OFF 0)
if ("${BUILD_SHARED_LIBS}" STREQUAL "ON")
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_SHARED_LIBS_AUTOMATIC_OFF 1)
endif()
add_subdirectory(${GTEST_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/gtest EXCLUDE_FROM_ALL)
if ("${BUILD_SHARED_LIBS_AUTOMATIC_OFF}" STREQUAL "1")
    set(BUILD_SHARED_LIBS ON)
    set(BUILD_SHARED_LIBS_AUTOMATIC_OFF 0)
endif()



##############################################################################
# build tests

enable_testing()
SET(TESTS_BAT "OFF" CACHE BOOL
    "Controls whether only BAT tests should be build or not")
SET(TEST_SYMLINK_CONFIG_FILES "OFF" CACHE BOOL
    "Controls if the json and scripts needed needed to run the tests are copied or symlinked into the build directroy (ignored on Windows)")
SET(TEST_SYMLINK_CONFIG_FILES_RELATIVE "OFF" CACHE BOOL
    "Controls if the json and scripts needed needed to run the tests are symlinked relatively into the build directroy (ignored on Windows)")

SET(TEST_IP_DEFAULT_VALUE "XXX.XXX.XXX.XXX")
SET(TEST_IP_MASTER "${TEST_IP_DEFAULT_VALUE}" CACHE STRING
    "The IP address of the interface which will act as test master")
SET(TEST_IP_SLAVE "${TEST_IP_DEFAULT_VALUE}" CACHE STRING
    "The IP address of the interface which will act as test slave")

if((${TEST_IP_MASTER} STREQUAL ${TEST_IP_DEFAULT_VALUE}) OR
   (${TEST_IP_SLAVE}  STREQUAL ${TEST_IP_DEFAULT_VALUE}))
    message(WARNING "TEST_IP_MASTER and/or TEST_IP_SLAVE isn't set. "
                    "Only local tests will be runnable "
                    "Please specify them via for example "
                    "-DTEST_IP_MASTER=10.0.3.1 -DTEST_IP_SLAVE=10.0.3.125")
endif()

SET(TEST_IP_SLAVE_SECOND "${TEST_IP_DEFAULT_VALUE}" CACHE STRING
    "The second IP address of the interface which will act as test slave")
set(TEST_SECOND_ADDRESS "OFF" CACHE BOOL
    "Controls whether second address tests should run or not")

if(${TEST_IP_SLAVE_SECOND} STREQUAL ${TEST_IP_DEFAULT_VALUE})
    message(WARNING "TEST_IP_SLAVE_SECOND isn't set. "
                    "Test with more than one IP address on same interface is not enabled."
                    "Please specify them via for example "
                    "-TEST_IP_SLAVE_SECOND=10.0.3.126")
else()
    set(TEST_SECOND_ADDRESS "ON")
endif()

set(TEST_E2E_PROFILE_04 "OFF" CACHE BOOL
    "Controls whether E2E Profile 04 tests should run or not")
if (ENABLE_SESSION_HANDLING_CONFIG)
    set(TEST_E2E_PROFILE_04 "ON")
else ()
    message(WARNING "ENABLE_SESSION_HANDLING_CONFIG isn't set. "
                    "Test of E2E Profile 04 is not enabled.")
endif ()


SET(TEST_UID_DEFAULT_VALUE "123456789")
SET(TEST_UID "${TEST_UID_DEFAULT_VALUE}" CACHE STRING
    "The User ID of the user running the test: Needed for security")
SET(TEST_GID_DEFAULT_VALUE "123456789")
SET(TEST_GID "${TEST_GID_DEFAULT_VALUE}" CACHE STRING
    "The Group ID of the user running the test: Needed for security")

SET(TEST_SECURITY "ON" CACHE BOOL
    "Controls whether security tests should run or not")

if((${TEST_UID} STREQUAL ${TEST_UID_DEFAULT_VALUE}) OR
   (${TEST_GID}  STREQUAL ${TEST_GID_DEFAULT_VALUE}))
    message(WARNING "TEST_UID and/or TEST_GID isn't set. "
                    "Security Tests are not runnable "
                    "Please specify them for example "
                    "-DTEST_UID=1000 -DTEST_GID=1000")
    SET(TEST_SECURITY "OFF")
endif()

add_custom_target(build_tests)
add_dependencies(build_tests vsomeip3)
add_dependencies(build_tests vsomeip3-sd)

set(CMAKE_CTEST_COMMAND ctest -V)
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})

add_dependencies(check build_tests)

##############################################################################
# add test directory

add_subdirectory( test EXCLUDE_FROM_ALL )
