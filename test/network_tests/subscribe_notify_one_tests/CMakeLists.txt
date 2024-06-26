# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(subscribe_notify_one_tests LANGUAGES CXX)

set(TEST_SUBSCRIBE_NOTIFY_ONE_NAME subscribe_notify_one_test)
set(TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_service)
add_executable(${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE}
    ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_service.cpp)
target_link_libraries(${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

# Copy config files for test into $BUILDDIR/test
set(TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE
    ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_master.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
    @ONLY)

set(TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE
    ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_slave.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
    @ONLY)

set(TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE
    ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_master_tcp.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}
    @ONLY)

set(TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE
    ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_slave_tcp.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}
    @ONLY)

set(TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE
    ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_master_udp.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE}
    @ONLY)

set(TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE
    ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_slave_udp.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE}
    @ONLY)

# copy starter scripts into builddir
# copy starter scripts into builddir
if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")
    set(TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_master_starter_qnx.sh)
else()
    set(TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_master_starter.sh)
endif()
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER}
    ${NETWORK_TEST_BIN_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER}
    ${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE}
)
set(TEST_SUBSCRIBE_NOTIFY_ONE_SLAVE_STARTER ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_slave_starter.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_SLAVE_STARTER}
    ${NETWORK_TEST_BIN_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_SLAVE_STARTER}
    ${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE}
)

add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER} UDP ${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE})
set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_tcp
    COMMAND ${NETWORK_TEST_BIN_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER} TCP ${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE})
set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_tcp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_both_tcp_and_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER} TCP_AND_UDP ${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE})
set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

add_dependencies(${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE} gtest)

add_dependencies(build_network_tests ${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE})
