# Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required (VERSION 2.8)
project (vsomeip)

set (VSOMEIP_MAJOR_VERSION 1)
set (VSOMEIP_MINOR_VERSION 3)
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
    set(OS_CXX_FLAGS "-D_GLIBCXX_USE_NANOSLEEP -pthread")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    set(OS "FREEBSD")
    set(DL_LIBRARY "")
    set(EXPORTSYMBOLS "")
    set(NO_DEPRECATED "-Wno-deprecated")
    set(OPTIMIZE "")
    set(OS_CXX_FLAGS "-pthread")
endif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")

include_directories(
	"interface" 
)
# Threads
find_package(Threads REQUIRED)

# Boost
find_package( Boost 1.54 COMPONENTS system thread log REQUIRED )
include_directories( ${Boost_INCLUDE_DIR} )

# Base library
file(GLOB vsomeip_SRC
    "implementation/configuration/src/*.cpp"
    "implementation/endpoints/src/*.cpp"
    "implementation/logging/src/*.cpp"
    "implementation/message/src/*.cpp"
    "implementation/routing/src/*.cpp"
    "implementation/runtime/src/*.cpp"
    "implementation/utility/src/*.cpp"
)

if (MSVC) 
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_WINSOCK_DEPRECATED_NO_WARNINGS -D_WIN32_WINNT=0x0501 -DWIN32 -DUSE_VSOMEIP_STATISTICS -DCOMMONAPI_INTERNAL_COMPILATION -DBOOST_LOG_DYN_LINK /EHsc")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_CRT_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS -D_WIN32_WINNT=0x0501 -DWIN32 -DUSE_VSOMEIP_STATISTICS -DCOMMONAPI_INTERNAL_COMPILATION -DBOOST_LOG_DYN_LINK /EHsc")
set(USE_RT "")
set(Boost_LIBRARIES "")
link_directories(${Boost_LIBRARY_DIR})
else()
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D${OS} ${OS_CXX_FLAGS} -DUSE_VSOMEIP_STATISTICS -DBOOST_LOG_DYN_LINK -g ${OPTIMIZE} -std=c++0x ${NO_DEPRECATED} ${EXPORTSYMBOLS}")
set(USE_RT "rt")
endif()

add_library(vsomeip SHARED ${vsomeip_SRC})
set_target_properties (vsomeip PROPERTIES VERSION ${VSOMEIP_VERSION} SOVERSION ${VSOMEIP_MAJOR_VERSION})
target_link_libraries(vsomeip ${Boost_LIBRARIES} ${USE_RT} ${DL_LIBRARY})

file(GLOB vsomeip-sd_SRC
	"implementation/service_discovery/src/*.cpp"
)

add_library(vsomeip-sd SHARED ${vsomeip-sd_SRC})
set_target_properties (vsomeip-sd PROPERTIES VERSION ${VSOMEIP_VERSION} SOVERSION ${VSOMEIP_MAJOR_VERSION})
target_link_libraries(vsomeip-sd vsomeip ${Boost_LIBRARIES} ${USE_RT} ${DL_LIBRARY})

# Configuration files

set(EXAMPLE_CONFIG_FILES
    "config/vsomeip.json"
    "config/vsomeip-local.json"
    "config/vsomeip-tcp-client.json"
    "config/vsomeip-tcp-service.json"
    "config/vsomeip-udp-client.json"
    "config/vsomeip-udp-service.json"
)

# Examples
add_executable(request-sample examples/request-sample.cpp ${EXAMPLE_CONFIG_FILES})
target_link_libraries(request-sample vsomeip ${Boost_LIBRARIES} ${DL_LIBRARY})

add_executable(response-sample examples/response-sample.cpp ${EXAMPLE_CONFIG_FILES})
target_link_libraries(response-sample vsomeip ${Boost_LIBRARIES} ${DL_LIBRARY})

add_executable(subscribe-sample examples/subscribe-sample.cpp ${EXAMPLE_CONFIG_FILES})
target_link_libraries(subscribe-sample vsomeip ${Boost_LIBRARIES} ${DL_LIBRARY})

add_executable(notify-sample examples/notify-sample.cpp ${EXAMPLE_CONFIG_FILES})
target_link_libraries(notify-sample vsomeip ${Boost_LIBRARIES} ${DL_LIBRARY})

###################################################################################################

file (GLOB_RECURSE vsomeip_INCLUDE "interface/*.hpp")

set_target_properties (vsomeip PROPERTIES PUBLIC_HEADER "${vsomeip_INCLUDE}")

install (
    TARGETS vsomeip 
    # IMPORTANT: Add the vsomeip library to the "export-set"
    EXPORT vsomeipTargets
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
    PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/vsomeip" COMPONENT dev
)

install (
    TARGETS vsomeip-sd
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
)

install (
    TARGETS request-sample response-sample subscribe-sample notify-sample
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
                               -o documentation/README.html
                               ${PROJECT_BINARY_DIR}/../README)
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

add_custom_target(build_tests)

set(CMAKE_CTEST_COMMAND ctest -V)
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})

add_dependencies(check build_tests)

##############################################################################
# add test directory

add_subdirectory( test EXCLUDE_FROM_ALL )
