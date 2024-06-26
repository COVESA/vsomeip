# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(header_factory_tests LANGUAGES CXX)

set(TEST_HEADER_FACTORY_NAME header_factory_test)
set(TEST_HEADER_FACTORY header_factory_test)
add_executable(${TEST_HEADER_FACTORY}
    header_factory_test.cpp)
target_link_libraries(${TEST_HEADER_FACTORY}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

##############################################################################
# Now comes the second part of the header factory test which consists of ouf
# a client and a service both with settings file and bash scripts to start them
set(TEST_HEADER_FACTORY_CLIENT header_factory_test_client)
add_executable(${TEST_HEADER_FACTORY_CLIENT}
    ${TEST_HEADER_FACTORY_CLIENT}.cpp)
target_link_libraries(${TEST_HEADER_FACTORY_CLIENT}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

# Copy config file for client into $BUILDDIR/test
set(TEST_HEADER_FACTORY_CLIENT_CONFIG_FILE ${TEST_HEADER_FACTORY_CLIENT}.json)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/header_factory_tests/${TEST_HEADER_FACTORY_CLIENT_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/header_factory_tests/${TEST_HEADER_FACTORY_CLIENT_CONFIG_FILE}
    ${TEST_HEADER_FACTORY_CLIENT}
)

# Copy bashscript to start client into $BUILDDIR/test
set(TEST_HEADER_FACTORY_CLIENT_START_SCRIPT ${TEST_HEADER_FACTORY_CLIENT}_start.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/header_factory_tests/${TEST_HEADER_FACTORY_CLIENT_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/header_factory_tests/${TEST_HEADER_FACTORY_CLIENT_START_SCRIPT}
    ${TEST_HEADER_FACTORY_CLIENT}
)

set(TEST_HEADER_FACTORY_SERVICE header_factory_test_service)
add_executable(${TEST_HEADER_FACTORY_SERVICE}
    ${TEST_HEADER_FACTORY_SERVICE}.cpp)
target_link_libraries(${TEST_HEADER_FACTORY_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

# Copy config file for service into $BUILDDIR/test
set(TEST_HEADER_FACTORY_SERVICE_CONFIG_FILE ${TEST_HEADER_FACTORY_SERVICE}.json)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/header_factory_tests/${TEST_HEADER_FACTORY_SERVICE_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/header_factory_tests/${TEST_HEADER_FACTORY_SERVICE_CONFIG_FILE}
    ${TEST_HEADER_FACTORY_SERVICE}
)

# Copy bashscript to start service into $BUILDDIR/test
set(TEST_HEADER_FACTORY_SERVICE_START_SCRIPT ${TEST_HEADER_FACTORY_SERVICE}_start.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/header_factory_tests/${TEST_HEADER_FACTORY_SERVICE_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/header_factory_tests/${TEST_HEADER_FACTORY_SERVICE_START_SCRIPT}
    ${TEST_HEADER_FACTORY_SERVICE}
)

# Copy bashscript to start client and server $BUILDDIR/test
set(TEST_HEADER_FACTORY_STARTER header_factory_test_send_receive_starter.sh)


configure_file(
    ${NETWORK_TEST_SRC_DIR}/header_factory_tests/conf/${TEST_HEADER_FACTORY_STARTER}.bat.in
    ${NETWORK_TEST_BIN_DIR}/header_factory_tests/${TEST_HEADER_FACTORY_STARTER}
    @ONLY)

# Header/Factory tets
add_test(NAME ${TEST_HEADER_FACTORY_NAME}
    COMMAND ${TEST_HEADER_FACTORY})
add_test(NAME ${TEST_HEADER_FACTORY_NAME}_send_receive
    COMMAND ${NETWORK_TEST_BIN_DIR}/header_factory_tests/${TEST_HEADER_FACTORY_STARTER}
)

add_dependencies(${TEST_HEADER_FACTORY} gtest)
add_dependencies(${TEST_HEADER_FACTORY_CLIENT} gtest)
add_dependencies(${TEST_HEADER_FACTORY_SERVICE} gtest)

add_dependencies(build_network_tests ${TEST_HEADER_FACTORY})
add_dependencies(build_network_tests ${TEST_HEADER_FACTORY_CLIENT})
add_dependencies(build_network_tests ${TEST_HEADER_FACTORY_SERVICE})
