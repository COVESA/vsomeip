# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(initial_event_tests LANGUAGES CXX)

set(TEST_INITIAL_EVENT_NAME initial_event_test)
set(TEST_INITIAL_EVENT_SERVICE ${TEST_INITIAL_EVENT_NAME}_service)
add_executable(${TEST_INITIAL_EVENT_SERVICE}
    ${TEST_INITIAL_EVENT_NAME}_service.cpp)
target_link_libraries(${TEST_INITIAL_EVENT_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

set(TEST_INITIAL_EVENT_CLIENT ${TEST_INITIAL_EVENT_NAME}_client)
add_executable(${TEST_INITIAL_EVENT_CLIENT}
    ${TEST_INITIAL_EVENT_NAME}_client.cpp)
target_link_libraries(${TEST_INITIAL_EVENT_CLIENT}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

set(TEST_INITIAL_EVENT_AVAILABILITY_CHECKER ${TEST_INITIAL_EVENT_NAME}_availability_checker)
add_executable(${TEST_INITIAL_EVENT_AVAILABILITY_CHECKER}
    ${TEST_INITIAL_EVENT_NAME}_availability_checker.cpp)
target_link_libraries(${TEST_INITIAL_EVENT_AVAILABILITY_CHECKER}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

set(TEST_INITIAL_EVENT_STOP_SERVICE ${TEST_INITIAL_EVENT_NAME}_stop_service)
add_executable(${TEST_INITIAL_EVENT_STOP_SERVICE}
    ${TEST_INITIAL_EVENT_NAME}_stop_service.cpp)
target_link_libraries(${TEST_INITIAL_EVENT_STOP_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

# Copy config files for test into $BUILDDIR/test
set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE
    ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_master.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
    @ONLY)

set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE
    ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_slave.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
    @ONLY)

# Copy config files for test into $BUILDDIR/test
set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE
    ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_master_tcp.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}
    @ONLY)

# Copy config files for test into $BUILDDIR/test
set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE
    ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_master_udp.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE}
    @ONLY)

set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE
    ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_slave_tcp.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}
    @ONLY)

set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE
    ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_slave_udp.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE}
    @ONLY)

set(TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE
    ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_master.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
    @ONLY)

set(TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE
    ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_slave.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
    @ONLY)

set(TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE
    ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_master_tcp.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE}
    @ONLY)

set(TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE
    ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_slave_tcp.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE}
    @ONLY)

set(TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE
    ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_master_udp.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE}
    @ONLY)

set(TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_UDP_CONFIG_FILE
    ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_slave_udp.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_UDP_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_UDP_CONFIG_FILE}
    @ONLY)

set(TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE
    ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_partial_same_ports_master.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}
    @ONLY)

set(TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE
    ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_partial_same_ports_slave.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}
    @ONLY)

set(TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE
    ${TEST_INITIAL_EVENT_NAME}_same_client_ids_same_ports_master.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
    @ONLY)

set(TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE
    ${TEST_INITIAL_EVENT_NAME}_same_client_ids_same_ports_slave.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
    @ONLY)

set(TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE
    ${TEST_INITIAL_EVENT_NAME}_same_client_ids_diff_ports_master.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
    @ONLY)

set(TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE
    ${TEST_INITIAL_EVENT_NAME}_same_client_ids_diff_ports_slave.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
    @ONLY)

set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE
    ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_same_service_id_master.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE}
    @ONLY)

set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_CONFIG_FILE
    ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_same_service_id_slave.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_CONFIG_FILE}
    @ONLY)

# copy starter scripts into builddir
if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")
    set(TEST_INITIAL_EVENT_MASTER_STARTER ${TEST_INITIAL_EVENT_NAME}_master_starter_qnx.sh)
else()
    set(TEST_INITIAL_EVENT_MASTER_STARTER ${TEST_INITIAL_EVENT_NAME}_master_starter.sh)
endif()

copy_to_builddir(${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER}
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER}
    ${TEST_INITIAL_EVENT_SERVICE}
)
set(TEST_INITIAL_EVENT_SLAVE_STARTER ${TEST_INITIAL_EVENT_NAME}_slave_starter.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_SLAVE_STARTER}
    ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_SLAVE_STARTER}
    ${TEST_INITIAL_EVENT_SERVICE}
)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE} UDP)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_tcp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE} TCP)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_tcp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_both_tcp_and_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE} TCP_AND_UDP)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE} UDP)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_tcp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE} TCP)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_tcp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_both_tcp_and_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE} TCP_AND_UDP)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_partial_same_ports_both_tcp_and_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE} TCP_AND_UDP)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_partial_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_same_service_id_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE} TCP_AND_UDP SAME_SERVICE_ID)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_same_service_id_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE} UDP MULTIPLE_EVENTS)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_tcp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE} TCP MULTIPLE_EVENTS)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_tcp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_both_tcp_and_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE} TCP_AND_UDP MULTIPLE_EVENTS)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE} UDP MULTIPLE_EVENTS)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_tcp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE} TCP MULTIPLE_EVENTS)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_tcp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_both_tcp_and_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE} TCP_AND_UDP MULTIPLE_EVENTS)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_partial_same_ports_both_tcp_and_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE} TCP_AND_UDP MULTIPLE_EVENTS)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_partial_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_same_service_id_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE} TCP_AND_UDP SAME_SERVICE_ID MULTIPLE_EVENTS)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_same_service_id_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE} UDP MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_tcp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE} TCP MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_tcp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_both_tcp_and_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE} TCP_AND_UDP MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE} UDP MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_tcp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE} TCP MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_tcp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_both_tcp_and_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE} TCP_AND_UDP MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_partial_same_ports_both_tcp_and_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE} TCP_AND_UDP MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_partial_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_same_service_id_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE} TCP_AND_UDP SAME_SERVICE_ID MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_same_service_id_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_partial_subscription_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE} UDP MULTIPLE_EVENTS SUBSCRIBE_ONLY_ONE)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_partial_subscription_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_partial_subscription_tcp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE} TCP MULTIPLE_EVENTS SUBSCRIBE_ONLY_ONE)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_partial_subscription_tcp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_partial_subscription_both_tcp_and_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE} TCP_AND_UDP MULTIPLE_EVENTS SUBSCRIBE_ONLY_ONE)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_partial_subscription_both_tcp_and_udp PROPERTIES TIMEOUT 120)

add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_udp_client_subscribes_twice
    COMMAND ${NETWORK_TEST_BIN_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE} UDP CLIENT_SUBSCRIBES_TWICE)
set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_udp_client_subscribes_twice PROPERTIES TIMEOUT 120)


add_dependencies(${TEST_INITIAL_EVENT_SERVICE} gtest)
add_dependencies(${TEST_INITIAL_EVENT_CLIENT} gtest)
add_dependencies(${TEST_INITIAL_EVENT_AVAILABILITY_CHECKER} gtest)
add_dependencies(${TEST_INITIAL_EVENT_STOP_SERVICE} gtest)

add_dependencies(build_network_tests ${TEST_INITIAL_EVENT_SERVICE})
add_dependencies(build_network_tests ${TEST_INITIAL_EVENT_CLIENT})
add_dependencies(build_network_tests ${TEST_INITIAL_EVENT_AVAILABILITY_CHECKER})
add_dependencies(build_network_tests ${TEST_INITIAL_EVENT_STOP_SERVICE})
