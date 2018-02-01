# Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.0.2)
project(vsomeip
    LANGUAGES CXX
    VERSION 2.6.2
    )

include(GNUInstallDirs)
include(GenerateExportHeader)
include(CMakePackageConfigHelpers)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/Modules")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(BUILD_SHARED_LIBS ON)

# Threads
find_package(Threads REQUIRED)

# Boost
find_package(Boost 1.55 COMPONENTS system thread log regex REQUIRED)
list(APPEND OS_COMPILER_DEFINITIONS -DBOOST_LOG_DYN_LINK)

# DLT
find_package(PkgConfig)
pkg_check_modules(DLT "automotive-dlt >= 2.11")
if(DLT_FOUND)
    list(APPEND OS_COMPILER_DEFINITIONS "-DUSE_DLT")
endif()

# SystemD
pkg_check_modules(SystemD "libsystemd")

if(NOT SystemD_FOUND)
    message(STATUS "Systemd was not found, watchdog disabled!")
    list(APPEND OS_COMPILER_DEFINITIONS "-DWITHOUT_SYSTEMD")
endif()

# Options
option(ENABLE_SIGNAL_HANDLING "Enable signal handling" OFF)

# Compatibility to new cmake
set(VSOMEIP_VERSION ${PROJECT_VERSION})
set(VSOMEIP_MAJOR_VERSION ${PROJECT_VERSION_MAJOR})

set(VSOMEIP_ROUTING "vsomeipd")
if(ROUTING)
    set(VSOMEIP_ROUTING ${ROUTING})
endif()
message("Predefined routing application: ${VSOMEIP_ROUTING}")

set(VSOMEIP_ROUTING_READY_MESSAGE "SOME/IP routing ready.")
if(ROUTING_READY_MESSAGE)
    set(VSOMEIP_ROUTING_READY_MESSAGE ${ROUTING_READY_MESSAGE})
endif()

set(VSOMEIP_DIAGNOSIS_ADDRESS "0x00")
if(DIAGNOSIS_ADDRESS)
    set(VSOMEIP_DIAGNOSIS_ADDRESS ${DIAGNOSIS_ADDRESS})
endif()
message("Predefined diagnosis address: ${VSOMEIP_DIAGNOSIS_ADDRESS}")

set(VSOMEIP_UNICAST_ADDRESS "127.0.0.1")
if(UNICAST_ADDRESS)
    set(VSOMEIP_UNICAST_ADDRESS ${UNICAST_ADDRESS})
endif()
message("Predefined unicast address: ${VSOMEIP_UNICAST_ADDRESS}")

# OS
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(OS "LINUX")
    set(OS_LINK_FLAGS -Wl,-export-dynamic -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exportmap.gcc)
    list(APPEND OS_COMPILER_DEFINITIONS -D_GLIBCXX_USE_NANOSLEEP -D_FORTIFY_SOURCE=2)
    list(APPEND OS_COMPILER_FLAGS -Wall -Wextra -Wformat -Wformat-security
    -Wconversion -fexceptions -fstrict-aliasing -fstack-protector
    -fasynchronous-unwind-tables -fno-omit-frame-pointer -O
    )
elseif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    set(OS "FREEBSD")
    set(OS_COMPILER_FLAGS -Wno-deprecated)
endif()

# Signal handling
if(ENABLE_SIGNAL_HANDLING)
    list(APPEND OS_COMPILER_DEFINITIONS "-DVSOMEIP_ENABLE_SIGNAL_HANDLING")
endif()

list(APPEND OS_COMPILER_DEFINITIONS -DVSOMEIP_VERSION="${VSOMEIP_VERSION}")

if(MSVC)
    message("using MSVC Compiler")
    set(BOOST_WINDOWS_VERSION "0x600" CACHE STRING
        "Set the same Version as the Version with which Boost was built, otherwise
        there will be errors. (normaly 0x600 is for Windows 7 and 0x501 is for Windows XP)"
        )
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_WINSOCK_DEPRECATED_NO_WARNING
        -D_WIN32_WINNT=${BOOST_WINDOWS_VERSION} -DWIN32 -DUSE_VSOMEIP_STATISTICS
        -DCOMMONAPI_INTERNAL_COMPILATION -DBOOST_LOG_DYN_LINK -DBOOST_ASIO_DISABLE_IOCP /EHsc"
        )
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS
        -D_WINSOCK_DEPRECATED_NO_WARNINGS -D_WIN32_WINNT=${BOOST_WINDOWS_VERSION} -DWIN32 -DUSE_VSOMEIP_STATISTICS
        -DCOMMONAPI_INTERNAL_COMPILATION -DBOOST_LOG_DYN_LINK -DBOOST_ASIO_DISABLE_IOCP /EHsc"
        )
    set(Boost_LIBRARIES "")
    link_directories(${Boost_LIBRARY_DIR_DEBUG})
    add_definitions(-DBOOST_ALL_DYN_LINK)
else()
    list(APPEND COMPILER_DEFINITIONS -DUSE_VSOMEIP_STATISTICS -DBOOST_LOG_DYN_LINK)
endif()

add_subdirectory(implementation)
add_subdirectory(config)
add_subdirectory(interface)
if(NOT MSVC)
add_subdirectory(daemon)
endif()
add_subdirectory(tools)
add_subdirectory(documentation)
add_subdirectory(test EXCLUDE_FROM_ALL)
add_subdirectory(examples EXCLUDE_FROM_ALL)
