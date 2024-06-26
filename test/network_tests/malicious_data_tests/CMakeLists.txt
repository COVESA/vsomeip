# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(malicious_data_tests LANGUAGES CXX)

set(TEST_MALICIOUS_DATA_NAME malicious_data_test)
set(TEST_MALICIOUS_DATA_SERVICE ${TEST_MALICIOUS_DATA_NAME}_service)
set(TEST_MALICIOUS_DATA_CLIENT ${TEST_MALICIOUS_DATA_NAME}_msg_sender)
set(TEST_MALICIOUS_DATA_MASTER_STARTER ${TEST_MALICIOUS_DATA_NAME}_master_starter.sh)
set(TEST_MALICIOUS_DATA_CONFIG_FILE ${TEST_MALICIOUS_DATA_NAME}_master.json)

add_executable(${TEST_MALICIOUS_DATA_SERVICE}
    ${TEST_MALICIOUS_DATA_NAME}_service.cpp)

target_link_libraries(${TEST_MALICIOUS_DATA_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

file(GLOB sd_sources
    "../../../implementation/service_discovery/src/*entry*.cpp"
    "../../../implementation/service_discovery/src/*option*.cpp"
    "../../../implementation/service_discovery/src/*message*.cpp"
)

add_executable(${TEST_MALICIOUS_DATA_CLIENT}
    ${TEST_MALICIOUS_DATA_CLIENT}.cpp
    ${PROJECT_SOURCE_DIR}/../../../implementation/message/src/deserializer.cpp
    ${PROJECT_SOURCE_DIR}/../../../implementation/message/src/message_impl.cpp
    ${PROJECT_SOURCE_DIR}/../../../implementation/message/src/payload_impl.cpp
    ${sd_sources}
)

target_link_libraries(${TEST_MALICIOUS_DATA_CLIENT}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    vsomeip_utilities
    ${VSOMEIP_NAME}
    ${VSOMEIP_NAME}-sd
    ${DLT_LIBRARIES}
)

# copy starter scripts into builddir
configure_file(
    ${NETWORK_TEST_SRC_DIR}/malicious_data_tests/conf/${TEST_MALICIOUS_DATA_MASTER_STARTER}.in
    ${NETWORK_TEST_BIN_DIR}/malicious_data_tests/${TEST_MALICIOUS_DATA_MASTER_STARTER}
    @ONLY)

# Copy config file for local test into $BUILDDIR/test
configure_file(
    ${NETWORK_TEST_SRC_DIR}/malicious_data_tests/conf/${TEST_MALICIOUS_DATA_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/malicious_data_tests/${TEST_MALICIOUS_DATA_CONFIG_FILE}
    @ONLY)

string(REPLACE "test_name" "${TEST_MALICIOUS_DATA_NAME}_events" TEST_MALICIOUS_DATA_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(
    NAME ${TEST_MALICIOUS_DATA_NAME}_events
    COMMAND ${TEST_MALICIOUS_DATA_ENTRYPOINT}
    ${NETWORK_TEST_BIN_DIR}/malicious_data_tests/${TEST_MALICIOUS_DATA_MASTER_STARTER} MALICIOUS_EVENTS)
set_tests_properties(${TEST_MALICIOUS_DATA_NAME}_events PROPERTIES TIMEOUT 180)

string(REPLACE "test_name" "${TEST_MALICIOUS_DATA_NAME}_protocol_version" TEST_MALICIOUS_DATA_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(
    NAME ${TEST_MALICIOUS_DATA_NAME}_protocol_version
    COMMAND ${TEST_MALICIOUS_DATA_ENTRYPOINT}
    ${NETWORK_TEST_BIN_DIR}/malicious_data_tests/${TEST_MALICIOUS_DATA_MASTER_STARTER} PROTOCOL_VERSION)
set_tests_properties(${TEST_MALICIOUS_DATA_NAME}_protocol_version PROPERTIES TIMEOUT 180)

string(REPLACE "test_name" "${TEST_MALICIOUS_DATA_NAME}_message_type" TEST_MALICIOUS_DATA_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(
    NAME ${TEST_MALICIOUS_DATA_NAME}_message_type
    COMMAND ${TEST_MALICIOUS_DATA_ENTRYPOINT}
    ${NETWORK_TEST_BIN_DIR}/malicious_data_tests/${TEST_MALICIOUS_DATA_MASTER_STARTER} MESSAGE_TYPE)
set_tests_properties(${TEST_MALICIOUS_DATA_NAME}_message_type PROPERTIES TIMEOUT 180)

string(REPLACE "test_name" "${TEST_MALICIOUS_DATA_NAME}_return_code" TEST_MALICIOUS_DATA_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(
    NAME ${TEST_MALICIOUS_DATA_NAME}_return_code
    COMMAND ${TEST_MALICIOUS_DATA_ENTRYPOINT}
    ${NETWORK_TEST_BIN_DIR}/malicious_data_tests/${TEST_MALICIOUS_DATA_MASTER_STARTER} RETURN_CODE)
set_tests_properties(${TEST_MALICIOUS_DATA_NAME}_return_code PROPERTIES TIMEOUT 180)

string(REPLACE "test_name" "${TEST_MALICIOUS_DATA_NAME}_wrong_header_fields_udp" TEST_MALICIOUS_DATA_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(
    NAME ${TEST_MALICIOUS_DATA_NAME}_wrong_header_fields_udp
    COMMAND ${TEST_MALICIOUS_DATA_ENTRYPOINT}
    ${NETWORK_TEST_BIN_DIR}/malicious_data_tests/${TEST_MALICIOUS_DATA_MASTER_STARTER} WRONG_HEADER_FIELDS_UDP)
set_tests_properties(${TEST_MALICIOUS_DATA_NAME}_wrong_header_fields_udp PROPERTIES TIMEOUT 180)

add_dependencies(${TEST_MALICIOUS_DATA_SERVICE} gtest)
add_dependencies(${TEST_MALICIOUS_DATA_CLIENT} gtest)

add_dependencies(build_network_tests ${TEST_MALICIOUS_DATA_SERVICE})
add_dependencies(build_network_tests ${TEST_MALICIOUS_DATA_CLIENT})
