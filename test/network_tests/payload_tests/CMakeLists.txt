# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(payload_tests LANGUAGES CXX)


set(TEST_LOCAL_PAYLOAD_NAME local_payload_test)
set(TEST_PAYLOAD_SERVICE payload_test_service)
add_executable(${TEST_PAYLOAD_SERVICE}
    ${TEST_PAYLOAD_SERVICE}.cpp)
target_link_libraries(${TEST_PAYLOAD_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

# Copy config file for service into $BUILDDIR/test
set(TEST_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE local_${TEST_PAYLOAD_SERVICE}.json)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE}
    ${TEST_PAYLOAD_SERVICE}
)

# Copy bashscript to start service into $BUILDDIR/test
set(TEST_LOCAL_PAYLOAD_SERVICE_START_SCRIPT local_${TEST_PAYLOAD_SERVICE}_start.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_LOCAL_PAYLOAD_SERVICE_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_LOCAL_PAYLOAD_SERVICE_START_SCRIPT}
    ${TEST_PAYLOAD_SERVICE}
)

set(TEST_PAYLOAD_CLIENT payload_test_client)
add_executable(${TEST_PAYLOAD_CLIENT}
    ${TEST_PAYLOAD_CLIENT}.cpp
    stopwatch.cpp
)
target_link_libraries(${TEST_PAYLOAD_CLIENT}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

# Copy config file for client into $BUILDDIR/test
set(TEST_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE local_${TEST_PAYLOAD_CLIENT}.json)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE}
    ${TEST_PAYLOAD_CLIENT}
)

# Copy bashscript to start client into $BUILDDIR/test
set(TEST_LOCAL_PAYLOAD_CLIENT_START_SCRIPT local_${TEST_PAYLOAD_CLIENT}_start.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_LOCAL_PAYLOAD_CLIENT_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_LOCAL_PAYLOAD_CLIENT_START_SCRIPT}
    ${TEST_PAYLOAD_CLIENT}
)

# Copy bashscript to start client and server $BUILDDIR/test
set(TEST_LOCAL_PAYLOAD_STARTER local_payload_test_starter.sh)

configure_file(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/conf/${TEST_LOCAL_PAYLOAD_STARTER}.bat.in
    ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_LOCAL_PAYLOAD_STARTER}
    @ONLY)

##############################################################################
set(TEST_EXTERNAL_LOCAL_PAYLOAD_NAME external_local_payload_test)
set(TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE external_local_payload_test_service)

# Copy config file for service into $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE ${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE}.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/conf/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE}
    @ONLY)

# Copy bashscript to start service into $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_START_SCRIPT ${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE}_start.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_START_SCRIPT}
    ${TEST_PAYLOAD_SERVICE}
)

set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL external_local_payload_test_client_local)

# Copy config file for client into $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL}.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/conf/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE}
    @ONLY)

# Copy bashscript to start client into $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_START_SCRIPT ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL}_start.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_START_SCRIPT}
    ${TEST_PAYLOAD_CLIENT}
)

# Copy bashscript to start client and server $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_NAME external_local_payload_test_client_local)
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_STARTER ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_NAME}_starter.sh)


configure_file(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/conf/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_STARTER}.bat.in
    ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_STARTER}
    @ONLY)

##############################################################################
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_NAME external_local_payload_test_client_external)
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL external_local_payload_test_client_external)

# Copy config file for client into $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_CONFIG_FILE ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL}.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/conf/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_CONFIG_FILE}
    @ONLY)

# Copy bashscript to start client into $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_START_SCRIPT ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL}_start.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_START_SCRIPT}
    ${TEST_PAYLOAD_CLIENT}
)

set(TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CLIENT_EXTERNAL external_local_payload_test_service_client_external)

# Copy bashscript to start service into $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CLIENT_EXTERNAL_START_SCRIPT ${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CLIENT_EXTERNAL}_start.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CLIENT_EXTERNAL_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CLIENT_EXTERNAL_START_SCRIPT}
    ${TEST_PAYLOAD_SERVICE}
)

# Copy bashscript to start client and server $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_STARTER ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_NAME}_starter.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_STARTER}
    ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_STARTER}
    ${TEST_PAYLOAD_CLIENT}
)

##############################################################################
# Copy bashscript to start client and server $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_NAME external_local_payload_test_client_local_and_external)
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_STARTER ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_NAME}_starter.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_STARTER}
    ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_STARTER}
    ${TEST_PAYLOAD_CLIENT}
)

##############################################################################
set(TEST_LOCAL_PAYLOAD_HUGE_NAME local_payload_test_huge_payload)
# Copy bashscript to start client and server $BUILDDIR/test
set(TEST_LOCAL_PAYLOAD_HUGE_STARTER ${TEST_LOCAL_PAYLOAD_HUGE_NAME}_starter.sh)

configure_file(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/conf/${TEST_LOCAL_PAYLOAD_HUGE_STARTER}.bat.in
    ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_LOCAL_PAYLOAD_HUGE_STARTER}
    @ONLY)

# Payload tests
add_test(NAME ${TEST_LOCAL_PAYLOAD_NAME}
    COMMAND ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_LOCAL_PAYLOAD_STARTER}
)
add_test(NAME ${TEST_LOCAL_PAYLOAD_HUGE_NAME}
    COMMAND ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_LOCAL_PAYLOAD_HUGE_STARTER}
)
set_tests_properties(${TEST_LOCAL_PAYLOAD_HUGE_NAME} PROPERTIES TIMEOUT 1800)
add_test(NAME ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_NAME}
    COMMAND ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_STARTER}
)
add_test(NAME ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_NAME}
    COMMAND ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_STARTER}
)
set_tests_properties(${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_NAME} PROPERTIES TIMEOUT 480)
add_test(NAME ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_NAME}
    COMMAND ${NETWORK_TEST_BIN_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_STARTER}
)
set_tests_properties(${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_NAME} PROPERTIES TIMEOUT 480)

add_dependencies(${TEST_PAYLOAD_SERVICE} gtest)
add_dependencies(${TEST_PAYLOAD_CLIENT} gtest)

add_dependencies(build_network_tests ${TEST_PAYLOAD_SERVICE})
add_dependencies(build_network_tests ${TEST_PAYLOAD_CLIENT})
