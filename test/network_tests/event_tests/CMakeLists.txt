# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(event_tests LANGUAGES CXX)

set(TEST_EVENT_NAME event_test)
set(TEST_EVENT_SERVICE ${TEST_EVENT_NAME}_service)
set(TEST_EVENT_CLIENT ${TEST_EVENT_NAME}_client)

add_executable(${TEST_EVENT_SERVICE}
    ${TEST_EVENT_SERVICE}.cpp)
target_link_libraries(${TEST_EVENT_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

add_executable(${TEST_EVENT_CLIENT}
    ${TEST_EVENT_CLIENT}.cpp
)
target_link_libraries(${TEST_EVENT_CLIENT}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)
# Copy service config file for external allow tests into $BUILDDIR/test
set(TEST_EVENT_SLAVE_TCP_CONFIG_FILE ${TEST_EVENT_NAME}_slave_tcp.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/event_tests/conf/${TEST_EVENT_SLAVE_TCP_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/event_tests/${TEST_EVENT_SLAVE_TCP_CONFIG_FILE}
    @ONLY)

set(TEST_EVENT_SLAVE_UDP_CONFIG_FILE ${TEST_EVENT_NAME}_slave_udp.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/event_tests/conf/${TEST_EVENT_SLAVE_UDP_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/event_tests/${TEST_EVENT_SLAVE_UDP_CONFIG_FILE}
    @ONLY)

set(TEST_EVENT_MASTER_CONFIG_FILE ${TEST_EVENT_NAME}_master.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/event_tests/conf/${TEST_EVENT_MASTER_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/event_tests/${TEST_EVENT_MASTER_CONFIG_FILE}
    @ONLY)

# Copy bashscript to start test (master) into $BUILDDIR/test
set(TEST_EVENT_MASTER_START_SCRIPT ${TEST_EVENT_NAME}_master_starter.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/event_tests/${TEST_EVENT_MASTER_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/event_tests/${TEST_EVENT_MASTER_START_SCRIPT}
    ${TEST_EVENT_SERVICE}
)

# Copy bashscript to start external tests (slave) into $BUILDDIR/test
set(TEST_EVENT_SLAVE_START_SCRIPT ${TEST_EVENT_NAME}_slave_starter.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/event_tests/${TEST_EVENT_SLAVE_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/event_tests/${TEST_EVENT_SLAVE_START_SCRIPT}
    ${TEST_EVENT_CLIENT}
)

add_test(NAME ${TEST_EVENT_NAME}_payload_fixed_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/event_tests/${TEST_EVENT_MASTER_START_SCRIPT} PAYLOAD_FIXED UDP)
set_tests_properties(${TEST_EVENT_NAME}_payload_fixed_udp PROPERTIES TIMEOUT 180)

add_test(NAME ${TEST_EVENT_NAME}_payload_fixed_tcp
    COMMAND ${NETWORK_TEST_BIN_DIR}/event_tests/${TEST_EVENT_MASTER_START_SCRIPT} PAYLOAD_FIXED TCP)
set_tests_properties(${TEST_EVENT_NAME}_payload_fixed_tcp PROPERTIES TIMEOUT 180)

add_test(NAME ${TEST_EVENT_NAME}_payload_dynamic_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/event_tests/${TEST_EVENT_MASTER_START_SCRIPT} PAYLOAD_DYNAMIC UDP)
set_tests_properties(${TEST_EVENT_NAME}_payload_dynamic_udp PROPERTIES TIMEOUT 180)

add_test(NAME ${TEST_EVENT_NAME}_payload_dynamic_tcp
    COMMAND ${NETWORK_TEST_BIN_DIR}/event_tests/${TEST_EVENT_MASTER_START_SCRIPT} PAYLOAD_DYNAMIC TCP)
set_tests_properties(${TEST_EVENT_NAME}_payload_dynamic_tcp PROPERTIES TIMEOUT 180)

add_dependencies(${TEST_EVENT_SERVICE} gtest)
add_dependencies(${TEST_EVENT_CLIENT} gtest)

add_dependencies(build_network_tests ${TEST_EVENT_SERVICE})
add_dependencies(build_network_tests ${TEST_EVENT_CLIENT})
