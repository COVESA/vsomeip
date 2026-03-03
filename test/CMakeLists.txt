# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

##############################################################################
# Test section
##############################################################################
set(CMAKE_CXX_STANDARD 20)

##############################################################################
# google benchmark
find_package(benchmark)

##############################################################################
# google test

# remove export symbols from the cxx flags
string(REPLACE "${EXPORTSYMBOLS}" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

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
# test libraries
# these expose all symbols, unlike the normal vsomeip libraries; useful for tests

add_library(${VSOMEIP_NAME}-test SHARED $<TARGET_OBJECTS:${VSOMEIP_NAME}-internal>)
# libvsomeip also `dlopens` other libs (cfg, sd, ..) so leave a rpath hint
set_target_properties(${VSOMEIP_NAME}-test PROPERTIES BUILD_RPATH "${CMAKE_CURRENT_BINARY_DIR}")
set_target_properties(${VSOMEIP_NAME}-test PROPERTIES OUTPUT_NAME ${VSOMEIP_NAME} VERSION ${VSOMEIP_VERSION} SOVERSION ${VSOMEIP_MAJOR_VERSION})
target_link_libraries(${VSOMEIP_NAME}-test PRIVATE ${Boost_LIBRARIES} ${USE_RT} ${DL_LIBRARY} ${DLT_LIBRARIES} ${SystemD_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

if (NOT MSVC)
    if(${CMAKE_SYSTEM_NAME} MATCHES "QNX")
        # add syscall wrappers to deal with CLOEXEC
        set_target_properties(${VSOMEIP_NAME}-test PROPERTIES LINK_FLAGS "-Wl,-wrap,socket -Wl,-wrap,accept -Wl,-wrap,open")
    else ()
        # add syscall wrappers to deal with CLOEXEC + react on certain errors
        set_target_properties(${VSOMEIP_NAME}-test PROPERTIES LINK_FLAGS "-Wl,-wrap,socket -Wl,-wrap,accept -Wl,-wrap,open -Wl,-wrap,close -Wl,-wrap,recvfrom -Wl,-wrap,recvmsg -Wl,-wrap,sendto -Wl,-wrap,sendmsg -Wl,-wrap,epoll_wait -Wl,-wrap,epoll_pwait")
    endif ()
endif ()

add_library(${VSOMEIP_NAME}-cfg-test SHARED $<TARGET_OBJECTS:${VSOMEIP_NAME}-cfg-internal>)
set_target_properties(${VSOMEIP_NAME}-cfg-test PROPERTIES OUTPUT_NAME ${VSOMEIP_NAME}-cfg VERSION ${VSOMEIP_VERSION} SOVERSION ${VSOMEIP_MAJOR_VERSION})
target_link_libraries(${VSOMEIP_NAME}-cfg-test ${VSOMEIP_NAME}-test ${Boost_LIBRARIES} ${USE_RT} ${DL_LIBRARY} ${SystemD_LIBRARIES})
set_target_properties(${VSOMEIP_NAME}-cfg-test PROPERTIES VERSION ${VSOMEIP_VERSION} SOVERSION ${VSOMEIP_MAJOR_VERSION})

add_library(${VSOMEIP_NAME}-sd-test SHARED $<TARGET_OBJECTS:${VSOMEIP_NAME}-sd-internal>)
set_target_properties(${VSOMEIP_NAME}-sd-test PROPERTIES OUTPUT_NAME ${VSOMEIP_NAME}-sd VERSION ${VSOMEIP_VERSION} SOVERSION ${VSOMEIP_MAJOR_VERSION})
target_link_libraries(${VSOMEIP_NAME}-sd-test ${VSOMEIP_NAME}-test ${Boost_LIBRARIES} ${USE_RT} ${DL_LIBRARY} ${SystemD_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

add_library(${VSOMEIP_NAME}-e2e-test SHARED $<TARGET_OBJECTS:${VSOMEIP_NAME}-e2e-internal>)
set_target_properties(${VSOMEIP_NAME}-e2e-test PROPERTIES OUTPUT_NAME ${VSOMEIP_NAME}-e2e VERSION ${VSOMEIP_VERSION} SOVERSION ${VSOMEIP_MAJOR_VERSION})
target_link_libraries(${VSOMEIP_NAME}-e2e-test ${VSOMEIP_NAME}-test ${Boost_LIBRARIES} ${USE_RT} ${DL_LIBRARY} ${SystemD_LIBRARIES})

if (MSVC)
    set_target_properties(${VSOMEIP_NAME}-test PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set_target_properties(${VSOMEIP_NAME}-test PROPERTIES COMPILE_DEFINITIONS "VSOMEIP_DLL_COMPILATION_PLUGIN")
    set_target_properties(${VSOMEIP_NAME}-cfg-test PROPERTIES COMPILE_DEFINITIONS "VSOMEIP_DLL_COMPILATION_PLUGIN")
    set_target_properties(${VSOMEIP_NAME}-sd-test PROPERTIES COMPILE_DEFINITIONS "VSOMEIP_DLL_COMPILATION_PLUGIN")
    set_target_properties(${VSOMEIP_NAME}-e2e-test PROPERTIES COMPILE_DEFINITIONS "VSOMEIP_DLL_COMPILATION_PLUGIN")
endif ()

##############################################################################
# build tests

SET(TESTS_BAT "OFF" CACHE BOOL
"Controls whether only BAT tests should be build or not")
SET(TEST_SYMLINK_CONFIG_FILES "OFF" CACHE BOOL
"Controls if the json and scripts needed needed to run the tests are copied or symlinked into the build directroy (ignored on Windows)")
SET(TEST_SYMLINK_CONFIG_FILES_RELATIVE "OFF" CACHE BOOL
    "Controls if the json and scripts needed needed to run the tests are symlinked relatively into the build directroy (ignored on Windows)")

SET(TEST_IP_DEFAULT_VALUE "127.0.0.1")
SET(TEST_IP_MASTER "${TEST_IP_DEFAULT_VALUE}" CACHE STRING
    "The IP address of the interface which will act as test master")
    SET(TEST_IP_SLAVE "${TEST_IP_DEFAULT_VALUE}" CACHE STRING
    "The IP address of the interface which will act as test slave")

if((${TEST_IP_MASTER} STREQUAL ${TEST_IP_DEFAULT_VALUE}) OR
   (${TEST_IP_SLAVE}  STREQUAL ${TEST_IP_DEFAULT_VALUE}))
    message(WARNING "TEST_IP_MASTER and/or TEST_IP_SLAVE isn't set. "
                    "Remote tests cannot be run. "
                    "To enable, please specify for example "
                    "-DTEST_IP_MASTER=10.0.3.1 -DTEST_IP_SLAVE=10.0.3.2")
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

set(TEST_E2E_PROFILE_04 "ON")
set(TEST_E2E_PROFILE_07 "ON")

SET(TEST_UID_DEFAULT_VALUE "123456789")
SET(TEST_UID "${TEST_UID_DEFAULT_VALUE}" CACHE STRING
    "The User ID of the user running the test: Needed for security")
SET(TEST_GID_DEFAULT_VALUE "123456789")
SET(TEST_GID "${TEST_GID_DEFAULT_VALUE}" CACHE STRING
"The Group ID of the user running the test: Needed for security")

SET(TEST_SECURITY "ON" CACHE BOOL
"Controls whether security tests should run or not")

if((${TEST_UID} STREQUAL ${TEST_UID_DEFAULT_VALUE}) OR
   (${TEST_GID}  STREQUAL ${TEST_GID_DEFAULT_VALUE}) OR
   DISABLE_SECURITY)
    message(WARNING "TEST_UID and/or TEST_GID isn't set. "
                    "Security Tests are not runnable "
                    "Please specify them for example "
                    "-DTEST_UID=1000 -DTEST_GID=1000")
    SET(TEST_SECURITY "OFF")
endif()

add_custom_target(build_tests)

set(CMAKE_CTEST_COMMAND ctest -V)
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
add_dependencies(check build_tests)

add_custom_target(build_network_tests)
add_dependencies(build_network_tests ${VSOMEIP_NAME}-test ${VSOMEIP_NAME}-cfg-test ${VSOMEIP_NAME}-e2e-test ${VSOMEIP_NAME}-sd-test)
add_dependencies(build_tests build_network_tests)

add_custom_target(build_unit_tests)
add_dependencies(build_unit_tests ${VSOMEIP_NAME}-test ${VSOMEIP_NAME}-cfg-test ${VSOMEIP_NAME}-e2e-test ${VSOMEIP_NAME}-sd-test)
add_dependencies(build_tests build_unit_tests)

add_custom_target(build_benchmark_tests)
add_dependencies(build_benchmark_tests ${VSOMEIP_NAME}-test ${VSOMEIP_NAME}-cfg-test ${VSOMEIP_NAME}-e2e-test ${VSOMEIP_NAME}-sd-test)
add_dependencies(build_tests build_benchmark_tests)

##############################################################################
# add network, unit and benchmark tests directories
##############################################################################

add_subdirectory(common EXCLUDE_FROM_ALL)
add_subdirectory(network_tests EXCLUDE_FROM_ALL)

if (NOT DISABLE_SECURITY)
    add_subdirectory(unit_tests EXCLUDE_FROM_ALL)

    if(benchmark_FOUND)
        add_subdirectory(benchmark_tests EXCLUDE_FROM_ALL)
    endif()
endif()

configure_file(tsan-suppressions.txt tsan-suppressions.txt COPYONLY)
