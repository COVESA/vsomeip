# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(restart_routing_tests LANGUAGES CXX)

set(TEST_RESTART_ROUTING_NAME restart_routing_test)
set(TEST_RESTART_ROUTING_SERVICE restart_routing_test_service)
add_executable(${TEST_RESTART_ROUTING_SERVICE}
    ${TEST_RESTART_ROUTING_SERVICE}.cpp)
target_link_libraries(${TEST_RESTART_ROUTING_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

# Copy config file for service into $BUILDDIR/test
set(TEST_RESTART_ROUTING_SERVICE_CONFIG_FILE ${TEST_RESTART_ROUTING_SERVICE}.json)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/restart_routing_tests/${TEST_RESTART_ROUTING_SERVICE_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/restart_routing_tests/${TEST_RESTART_ROUTING_SERVICE_CONFIG_FILE}
    ${TEST_RESTART_ROUTING_SERVICE}
)

# Copy bashscript to start service into $BUILDDIR/test
set(TEST_RESTART_ROUTING_SERVICE_START_SCRIPT ${TEST_RESTART_ROUTING_SERVICE}_start.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/restart_routing_tests/${TEST_RESTART_ROUTING_SERVICE_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/restart_routing_tests/${TEST_RESTART_ROUTING_SERVICE_START_SCRIPT}
    ${TEST_RESTART_ROUTING_SERVICE}
)

set(TEST_RESTART_ROUTING_CLIENT restart_routing_test_client)
add_executable(${TEST_RESTART_ROUTING_CLIENT}
    ${TEST_RESTART_ROUTING_CLIENT}.cpp
)
target_link_libraries(${TEST_RESTART_ROUTING_CLIENT}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

# Copy config file for client into $BUILDDIR/test
set(TEST_RESTART_ROUTING_CLIENT_CONFIG_FILE ${TEST_RESTART_ROUTING_CLIENT}.json)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/restart_routing_tests/${TEST_RESTART_ROUTING_CLIENT_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/restart_routing_tests/${TEST_RESTART_ROUTING_CLIENT_CONFIG_FILE}
    ${TEST_RESTART_ROUTING_CLIENT}
)

# Copy bashscript to start client into $BUILDDIR/test
set(TEST_RESTART_ROUTING_CLIENT_START_SCRIPT ${TEST_RESTART_ROUTING_CLIENT}_start.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/restart_routing_tests/${TEST_RESTART_ROUTING_CLIENT_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/restart_routing_tests/${TEST_RESTART_ROUTING_CLIENT_START_SCRIPT}
    ${TEST_RESTART_ROUTING_CLIENT}
)

# Copy bashscript to start client and server $BUILDDIR/test
set(TEST_RESTART_ROUTING_STARTER restart_routing_test_starter.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/restart_routing_tests/${TEST_RESTART_ROUTING_STARTER}
    ${NETWORK_TEST_BIN_DIR}/restart_routing_tests/${TEST_RESTART_ROUTING_STARTER}
    ${TEST_RESTART_ROUTING_CLIENT}
)

# Copy config file for autoconfig into $BUILDDIR/test
set(TEST_RESTART_ROUTING_AUTO_CONFIG_FILE ${TEST_RESTART_ROUTING_NAME}_autoconfig.json)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/restart_routing_tests/${TEST_RESTART_ROUTING_AUTO_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/restart_routing_tests/${TEST_RESTART_ROUTING_AUTO_CONFIG_FILE}
    ${TEST_RESTART_ROUTING_CLIENT}
)

add_test(NAME ${TEST_RESTART_ROUTING_NAME}
    COMMAND ${NETWORK_TEST_BIN_DIR}/restart_routing_tests/${TEST_RESTART_ROUTING_STARTER}
)

add_dependencies(build_network_tests ${TEST_RESTART_ROUTING_SERVICE})
add_dependencies(build_network_tests ${TEST_RESTART_ROUTING_CLIENT})
