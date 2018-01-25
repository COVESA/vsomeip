# Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required (VERSION 2.8.12)
project (vsomeip)

set (VSOMEIP_MAJOR_VERSION 2)
set (VSOMEIP_MINOR_VERSION 8)
set (VSOMEIP_PATCH_VERSION 0)
set (VSOMEIP_VERSION ${VSOMEIP_MAJOR_VERSION}.${VSOMEIP_MINOR_VERSION}.${VSOMEIP_PATCH_VERSION})
set (PACKAGE_VERSION ${VSOMEIP_VERSION}) # Used in documentatin/doxygen.in
set (CMAKE_VERBOSE_MAKEFILE off)

###################################################################################################
# see http://www.cmake.org/Wiki/CMake/Tutorials/How_to_create_a_ProjectConfig.cmake_file
###################################################################################################

# Offer the user the choice of overriding the installation directories
set (INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set (INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
set (INSTALL_INCLUDE_DIR include CACHE PATH "Installation directory for header files")

if (WIN32 AND NOT CYGWIN)
    set (DEF_INSTALL_CMAKE_DIR CMake)
else ()
    set (DEF_INSTALL_CMAKE_DIR lib/cmake/vsomeip)
endif ()

set (INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH "Installation directory for CMake files")

# Make relative paths absolute (needed later on)
foreach (p LIB BIN INCLUDE CMAKE)
    set (var INSTALL_${p}_DIR)
    if (NOT IS_ABSOLUTE "${${var}}")
        set (ABSOLUTE_${var} "${CMAKE_INSTALL_PREFIX}/${${var}}") # Add all targets to the build-tree export set
    endif ()
endforeach ()

###################################################################################################

# OS
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(OS "LINUX")
    set(DL_LIBRARY "dl")
    set(EXPORTSYMBOLS "-Wl,-export-dynamic -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exportmap.gcc")
    set(NO_DEPRECATED "")
    set(OPTIMIZE "")
    set(OS_CXX_FLAGS "-D_GLIBCXX_USE_NANOSLEEP -pthread -O -Wall -Wextra -Wformat -Wformat-security -Wconversion -fexceptions -fstrict-aliasing -fstack-protector -fasynchronous-unwind-tables -fno-omit-frame-pointer -D_FORTIFY_SOURCE=2")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    set(OS "FREEBSD")
    set(DL_LIBRARY "")
    set(EXPORTSYMBOLS "")
    set(NO_DEPRECATED "-Wno-deprecated")
    set(OPTIMIZE "")
    set(OS_CXX_FLAGS "-pthread")
endif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")

# Signal handling
if (ENABLE_SIGNAL_HANDLING)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVSOMEIP_ENABLE_SIGNAL_HANDLING")
endif ()

include_directories(
    "interface" 
)

# Threads
find_package(Threads REQUIRED)

# Boost
find_package( Boost 1.55 COMPONENTS system thread log REQUIRED )
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

# DLT
find_package(PkgConfig)
pkg_check_modules(DLT "automotive-dlt >= 2.11")
IF(DLT_FOUND) 
     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_DLT")
ENDIF(DLT_FOUND)

# SystemD
pkg_check_modules(SystemD "libsystemd")

if(NOT SystemD_FOUND)
MESSAGE( STATUS "Systemd was not found, watchdog disabled!")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWITHOUT_SYSTEMD")
endif(NOT SystemD_FOUND)

include_directories(
    include
    implementation/helper
    ${DLT_INCLUDE_DIRS}
)

link_directories(
    ${DLT_LIBDIR}
)

# Base library
file(GLOB vsomeip_SRC
    "implementation/endpoints/src/*.cpp"
    "implementation/logging/src/*.cpp"
    "implementation/tracing/src/*.cpp"
    "implementation/message/src/*.cpp"
    "implementation/routing/src/*.cpp"
    "implementation/runtime/src/*.cpp"
    "implementation/utility/src/*.cpp"
    "implementation/plugin/src/*.cpp"
)

file(GLOB_RECURSE vsomeip_e2e_SRC
    "implementation/e2e_protection/src/*.cpp"
)

list(SORT vsomeip_SRC)
list(SORT vsomeip_e2e_SRC)

add_definitions(-DVSOMEIP_VERSION="${VSOMEIP_VERSION}")

if (MSVC)
    message("using MSVC Compiler")
    # add_definitions(-DVSOMEIP_DLL_COMPILATION) now it is controlled per target
    SET(BOOST_WINDOWS_VERSION "0x600" CACHE STRING "Set the same Version as the Version with which Boost was built, otherwise there will be errors. (normaly 0x600 is for Windows 7 and 0x501 is for Windows XP)")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_WINSOCK_DEPRECATED_NO_WARNINGS -D_WIN32_WINNT=${BOOST_WINDOWS_VERSION} -DWIN32 -DUSE_VSOMEIP_STATISTICS -DCOMMONAPI_INTERNAL_COMPILATION -DBOOST_LOG_DYN_LINK -DBOOST_ASIO_DISABLE_IOCP /EHsc")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS -D_WIN32_WINNT=${BOOST_WINDOWS_VERSION} -DWIN32 -DUSE_VSOMEIP_STATISTICS -DCOMMONAPI_INTERNAL_COMPILATION -DBOOST_LOG_DYN_LINK -DBOOST_ASIO_DISABLE_IOCP /EHsc")
    set(USE_RT "")
    set(Boost_LIBRARIES "")
    link_directories(${Boost_LIBRARY_DIR_DEBUG})
    ADD_DEFINITIONS( -DBOOST_ALL_DYN_LINK )
else()
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D${OS} ${OS_CXX_FLAGS} -DUSE_VSOMEIP_STATISTICS -DBOOST_LOG_DYN_LINK -g ${OPTIMIZE} -std=c++0x ${NO_DEPRECATED} ${EXPORTSYMBOLS}")
    set(USE_RT "rt")
endif()

add_library(vsomeip SHARED ${vsomeip_SRC} ${vsomeip_e2e_SRC})
set_target_properties (vsomeip PROPERTIES VERSION ${VSOMEIP_VERSION} SOVERSION ${VSOMEIP_MAJOR_VERSION})
# PRIVATE means the listed libraries won't be included in the "link interface",
# meaning the exported vsomeipTargets.cmake targets won't try to link against
# them (which shouldn't be required). ${Boost_LIBRARIES} includes absolute
# build host paths as of writing, which also makes this important as it breaks
# the build.
target_link_libraries(vsomeip PRIVATE ${Boost_LIBRARIES} ${USE_RT} ${DL_LIBRARY} ${DLT_LIBRARIES} ${SystemD_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

# Configuration library
file(GLOB vsomeip-cfg_SRC
    "implementation/configuration/src/*.cpp"
)
list(SORT vsomeip-cfg_SRC)

add_library(vsomeip-cfg SHARED ${vsomeip-cfg_SRC})
set_target_properties (vsomeip-cfg PROPERTIES VERSION ${VSOMEIP_VERSION} SOVERSION ${VSOMEIP_MAJOR_VERSION})
target_link_libraries(vsomeip-cfg vsomeip ${Boost_LIBRARIES} ${USE_RT} ${DL_LIBRARY} ${SystemD_LIBRARIES})

# Service-Discovery library
file(GLOB vsomeip-sd_SRC
    "implementation/service_discovery/src/*.cpp"
)
list(SORT vsomeip-sd_SRC)

add_library(vsomeip-sd SHARED ${vsomeip-sd_SRC})
set_target_properties (vsomeip-sd PROPERTIES VERSION ${VSOMEIP_VERSION} SOVERSION ${VSOMEIP_MAJOR_VERSION})
target_link_libraries(vsomeip-sd vsomeip ${Boost_LIBRARIES} ${USE_RT} ${DL_LIBRARY} ${SystemD_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

if (MSVC)
    set_target_properties(vsomeip-cfg PROPERTIES COMPILE_DEFINITIONS "VSOMEIP_DLL_COMPILATION_PLUGIN")
    set_target_properties(vsomeip-sd PROPERTIES COMPILE_DEFINITIONS "VSOMEIP_DLL_COMPILATION_PLUGIN")
    set_target_properties(vsomeip PROPERTIES COMPILE_DEFINITIONS "VSOMEIP_DLL_COMPILATION")
endif()

# Configuration files

set(EXAMPLE_CONFIG_FILES
    "config/vsomeip.json"
    "config/vsomeip-local.json"
    "config/vsomeip-tcp-client.json"
    "config/vsomeip-tcp-service.json"
    "config/vsomeip-udp-client.json"
    "config/vsomeip-udp-service.json"
)

###################################################################################################
set (VSOMEIP_DIAGNOSIS_ADDRESS "0x00")
if (DIAGNOSIS_ADDRESS)
set (VSOMEIP_DIAGNOSIS_ADDRESS ${DIAGNOSIS_ADDRESS})
endif ()
set (VSOMEIP_UNICAST_ADDRESS "127.0.0.1")
if (UNICAST_ADDRESS)
set (VSOMEIP_UNICAST_ADDRESS ${UNICAST_ADDRESS})
endif ()
set (VSOMEIP_ROUTING "vsomeipd")
if (ROUTING)
set (VSOMEIP_ROUTING ${ROUTING})
endif ()
set (VSOMEIP_ROUTING_READY_MESSAGE "SOME/IP routing ready.")
if (ROUTING_READY_MESSAGE)
set (VSOMEIP_ROUTING_READY_MESSAGE ${ROUTING_READY_MESSAGE})
endif ()
message("Predefined unicast address: ${VSOMEIP_UNICAST_ADDRESS}")
message("Predefined diagnosis address: ${VSOMEIP_DIAGNOSIS_ADDRESS}")
message("Predefined routing application: ${VSOMEIP_ROUTING}")

###################################################################################################

set(INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/interface/vsomeip")

file (GLOB_RECURSE vsomeip_INCLUDE RELATIVE ${INCLUDE_PATH} "interface/*.hpp"  )
list (SORT vsomeip_INCLUDE)

foreach ( file ${vsomeip_INCLUDE} )
    get_filename_component( dir ${file} DIRECTORY )
    install( FILES "${INCLUDE_PATH}/${file}" DESTINATION "${INSTALL_INCLUDE_DIR}/vsomeip/${dir}" COMPONENT dev)
endforeach()

install (
    TARGETS vsomeip 
    # IMPORTANT: Add the vsomeip library to the "export-set"
    EXPORT vsomeipTargets
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT
    ARCHIVE DESTINATION "${INSTALL_LIB_DIR}"
    COMPONENT dev
)

install (
    TARGETS vsomeip-cfg
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
)

install (
    TARGETS vsomeip-sd
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
)

install (
  FILES ${EXAMPLE_CONFIG_FILES} DESTINATION etc/vsomeip COMPONENT config
)

# Add all targets to the build-tree export set
export (TARGETS vsomeip FILE "${PROJECT_BINARY_DIR}/vsomeipTargets.cmake")

# Export the package for use from the build-tree
# (this registers the build-tree with a global CMake-registry)
export (PACKAGE vsomeip)

# Create the vsomeipConfig.cmake and vsomeipConfigVersion files
file (RELATIVE_PATH REL_INCLUDE_DIR "${ABSOLUTE_INSTALL_CMAKE_DIR}" "${ABSOLUTE_INSTALL_INCLUDE_DIR}")

# ... for the build tree
set (CONF_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/interface" "${PROJECT_BINARY_DIR}")
configure_file (vsomeipConfig.cmake.in "${PROJECT_BINARY_DIR}/vsomeipConfig.cmake" @ONLY)

# ... for the install tree
set (CONF_INCLUDE_DIRS "\${VSOMEIP_CMAKE_DIR}/${REL_INCLUDE_DIR}")
configure_file (vsomeipConfig.cmake.in "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/vsomeipConfig.cmake" @ONLY)

# ... for both
configure_file (vsomeipConfigVersion.cmake.in "${PROJECT_BINARY_DIR}/vsomeipConfigVersion.cmake" @ONLY)

# confugure internal.hpp for correct version number
configure_file (
   "${PROJECT_SOURCE_DIR}/implementation/configuration/include/internal.hpp.in"
   "${PROJECT_SOURCE_DIR}/implementation/configuration/include/internal.hpp"
)

# Install the vsomeipConfig.cmake and vsomeipConfigVersion.cmake
install (
    FILES
    "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/vsomeipConfig.cmake"
    "${PROJECT_BINARY_DIR}/vsomeipConfigVersion.cmake"
    DESTINATION "${INSTALL_CMAKE_DIR}"
    COMPONENT dev
)

# Install the export set for use with the install-tree
install (
    EXPORT vsomeipTargets
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
                               ${PROJECT_BINARY_DIR}/../documentation/vsomeipUserGuide)
endif()

##############################################################################
# create pkg-config file
if(NOT WIN32)
        configure_file(vsomeip.pc.in ${PROJECT_BINARY_DIR}/vsomeip.pc @ONLY)
        install(FILES ${PROJECT_BINARY_DIR}/vsomeip.pc DESTINATION lib/pkgconfig)
endif()

##############################################################################
# build daemon (Non-Windows only)
if (NOT MSVC)
add_custom_target( daemon )
add_subdirectory( daemon )
endif()

# build tools
add_custom_target( tools )
add_subdirectory( tools )

# build examples
add_custom_target( examples )
add_subdirectory( examples EXCLUDE_FROM_ALL )

# build plugins located directly in the build tree
# (Non-Windows only)
if (NOT MSVC)
    if(EXISTS "${PROJECT_SOURCE_DIR}/plugins/CMakeLists.txt")
       # build plugins if available
       message("Plugins CMakeList.txt found: Build plugins")
       add_subdirectory( plugins )
    endif()
endif()


##############################################################################
# Test section
##############################################################################

##############################################################################
# google test

# check for set environment variable
if(DEFINED ENV{GTEST_ROOT})
    message("GTEST_ROOT is set. GTEST_ROOT = $ENV{GTEST_ROOT}")
else()
    message("GTEST_ROOT is not defined. For building the tests environment variable
             GTEST_ROOT have to be defined. Tests can not be built.")
    # early exit
    return()  # test can not be build -> make commands build_tests and check are not available
endif()

# build google test as static library (always) -> therefore deactivate BUILD_SHARED_LIBS in case it is active
set(BUILD_SHARED_LIBS_AUTOMATIC_OFF 0)
if ("${BUILD_SHARED_LIBS}" STREQUAL "ON")
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_SHARED_LIBS_AUTOMATIC_OFF 1)
endif()
add_subdirectory($ENV{GTEST_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/gtest EXCLUDE_FROM_ALL)
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

add_custom_target(build_tests)
add_dependencies(build_tests vsomeip)
add_dependencies(build_tests vsomeip-sd)

set(CMAKE_CTEST_COMMAND ctest -V)
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})

add_dependencies(check build_tests)

##############################################################################
# add test directory

add_subdirectory( test EXCLUDE_FROM_ALL )
