# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(second_address_tests LANGUAGES CXX)

set(TEST_SECOND_ADDRESS_NAME second_address_test)
set(TEST_SECOND_ADDRESS_SERVICE ${TEST_SECOND_ADDRESS_NAME}_service)
set(TEST_SECOND_ADDRESS_CLIENT ${TEST_SECOND_ADDRESS_NAME}_client)

add_executable(${TEST_SECOND_ADDRESS_SERVICE}
    ${TEST_SECOND_ADDRESS_SERVICE}.cpp
)
target_link_libraries(${TEST_SECOND_ADDRESS_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

add_executable(${TEST_SECOND_ADDRESS_CLIENT}
    ${TEST_SECOND_ADDRESS_CLIENT}.cpp
)
target_link_libraries(${TEST_SECOND_ADDRESS_CLIENT}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

set(TEST_SECOND_ADDRESS_MASTER_SERVICE_UDP_CONFIG_FILE ${TEST_SECOND_ADDRESS_NAME}_master_service_udp.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/second_address_tests/conf/${TEST_SECOND_ADDRESS_MASTER_SERVICE_UDP_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/second_address_tests/${TEST_SECOND_ADDRESS_MASTER_SERVICE_UDP_CONFIG_FILE}
    @ONLY)

set(TEST_SECOND_ADDRESS_SLAVE_CLIENT_CONFIG_FILE ${TEST_SECOND_ADDRESS_NAME}_slave_client.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/second_address_tests/conf/${TEST_SECOND_ADDRESS_SLAVE_CLIENT_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/second_address_tests/${TEST_SECOND_ADDRESS_SLAVE_CLIENT_CONFIG_FILE}
    @ONLY)

set(TEST_SECOND_ADDRESS_MASTER_CLIENT_CONFIG_FILE ${TEST_SECOND_ADDRESS_NAME}_master_client.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/second_address_tests/conf/${TEST_SECOND_ADDRESS_MASTER_CLIENT_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/second_address_tests/${TEST_SECOND_ADDRESS_MASTER_CLIENT_CONFIG_FILE}
    @ONLY)

set(TEST_SECOND_ADDRESS_SLAVE_SERVICE_UDP_CONFIG_FILE ${TEST_SECOND_ADDRESS_NAME}_slave_service_udp.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/second_address_tests/conf/${TEST_SECOND_ADDRESS_SLAVE_SERVICE_UDP_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/second_address_tests/${TEST_SECOND_ADDRESS_SLAVE_SERVICE_UDP_CONFIG_FILE}
    @ONLY)

set(TEST_SECOND_ADDRESS_MASTER_START_SCRIPT ${TEST_SECOND_ADDRESS_NAME}_master_starter.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/second_address_tests/${TEST_SECOND_ADDRESS_MASTER_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/second_address_tests/${TEST_SECOND_ADDRESS_MASTER_START_SCRIPT}
    ${TEST_SECOND_ADDRESS_SERVICE}
)

set(TEST_SECOND_ADDRESS_SLAVE_START_SCRIPT ${TEST_SECOND_ADDRESS_NAME}_slave_starter.sh)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/second_address_tests/conf/${TEST_SECOND_ADDRESS_SLAVE_START_SCRIPT}.in
    ${NETWORK_TEST_BIN_DIR}/second_address_tests/${TEST_SECOND_ADDRESS_SLAVE_START_SCRIPT}
    @ONLY)

string(REPLACE "test_name" "${TEST_SECOND_ADDRESS_NAME}_second_ip_address_service_udp" TEST_SECOND_ADDRESS_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(NAME ${TEST_SECOND_ADDRESS_NAME}_second_ip_address_service_udp
    COMMAND ${TEST_SECOND_ADDRESS_ENTRYPOINT}
    ${NETWORK_TEST_BIN_DIR}/second_address_tests/${TEST_SECOND_ADDRESS_MASTER_START_SCRIPT} SERVICE UDP
)
set_tests_properties(${TEST_SECOND_ADDRESS_NAME}_second_ip_address_service_udp PROPERTIES TIMEOUT 180)

string(REPLACE "test_name" "${TEST_SECOND_ADDRESS_NAME}_second_ip_address_client_udp" TEST_SECOND_ADDRESS_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(NAME ${TEST_SECOND_ADDRESS_NAME}_second_ip_address_client_udp
    COMMAND ${TEST_SECOND_ADDRESS_ENTRYPOINT}
    ${NETWORK_TEST_BIN_DIR}/second_address_tests/${TEST_SECOND_ADDRESS_MASTER_START_SCRIPT} CLIENT UDP
)
set_tests_properties(${TEST_SECOND_ADDRESS_NAME}_second_ip_address_client_udp PROPERTIES TIMEOUT 180)

add_dependencies(${TEST_SECOND_ADDRESS_CLIENT} gtest)
add_dependencies(${TEST_SECOND_ADDRESS_SERVICE} gtest)

add_dependencies(build_network_tests ${TEST_SECOND_ADDRESS_CLIENT})
add_dependencies(build_network_tests ${TEST_SECOND_ADDRESS_SERVICE})
