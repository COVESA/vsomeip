# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(pending_subscription_test LANGUAGES CXX)

set(TEST_PENDING_SUBSCRIPTION_NAME pending_subscription_test)
set(TEST_PENDING_SUBSCRIPTION_SERVICE ${TEST_PENDING_SUBSCRIPTION_NAME}_service)
set(TEST_PENDING_SUBSCRIPTION_CLIENT ${TEST_PENDING_SUBSCRIPTION_NAME}_sd_msg_sender)
set(TEST_PENDING_SUBSCRIPTION_MASTER_STARTER ${TEST_PENDING_SUBSCRIPTION_NAME}_master_starter.sh)
set(TEST_PENDING_SUBSCRIPTION_CONFIG_FILE ${TEST_PENDING_SUBSCRIPTION_NAME}_master.json)

add_executable(${TEST_PENDING_SUBSCRIPTION_SERVICE}
    ${TEST_PENDING_SUBSCRIPTION_SERVICE}.cpp)

target_link_libraries(${TEST_PENDING_SUBSCRIPTION_SERVICE}
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
add_executable(${TEST_PENDING_SUBSCRIPTION_CLIENT}
    ${TEST_PENDING_SUBSCRIPTION_CLIENT}.cpp
    ${PROJECT_SOURCE_DIR}/../../../implementation/message/src/deserializer.cpp
    ${PROJECT_SOURCE_DIR}/../../../implementation/message/src/message_impl.cpp
    ${PROJECT_SOURCE_DIR}/../../../implementation/message/src/payload_impl.cpp
    ${sd_sources}
)

target_link_libraries(${TEST_PENDING_SUBSCRIPTION_CLIENT}
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
    ${NETWORK_TEST_SRC_DIR}/pending_subscription_tests/conf/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER}.in
    ${NETWORK_TEST_BIN_DIR}/pending_subscription_tests/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER}
    @ONLY)

# Copy config file for local test into $BUILDDIR/test
configure_file(
    ${NETWORK_TEST_SRC_DIR}/pending_subscription_tests/conf/${TEST_PENDING_SUBSCRIPTION_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/pending_subscription_tests/${TEST_PENDING_SUBSCRIPTION_CONFIG_FILE}
    @ONLY)

enable_testing()
string(REPLACE "test_name" "${TEST_PENDING_SUBSCRIPTION_NAME}_subscribe" TEST_PENDING_SUBSCRIPTION_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(
    NAME ${TEST_PENDING_SUBSCRIPTION_NAME}_subscribe
    COMMAND ${TEST_PENDING_SUBSCRIPTION_ENTRYPOINT}
    ${NETWORK_TEST_BIN_DIR}/pending_subscription_tests/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER} SUBSCRIBE)
set_tests_properties(${TEST_PENDING_SUBSCRIPTION_NAME}_subscribe PROPERTIES TIMEOUT 180)

string(REPLACE "test_name" "${TEST_PENDING_SUBSCRIPTION_NAME}_alternating_subscribe_unsubscribe" TEST_PENDING_SUBSCRIPTION_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(
    NAME ${TEST_PENDING_SUBSCRIPTION_NAME}_alternating_subscribe_unsubscribe
    COMMAND ${TEST_PENDING_SUBSCRIPTION_ENTRYPOINT}
    ${NETWORK_TEST_BIN_DIR}/pending_subscription_tests/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER} SUBSCRIBE_UNSUBSCRIBE)
set_tests_properties(${TEST_PENDING_SUBSCRIPTION_NAME}_alternating_subscribe_unsubscribe PROPERTIES TIMEOUT 180)

string(REPLACE "test_name" "${TEST_PENDING_SUBSCRIPTION_NAME}_unsubscribe" TEST_PENDING_SUBSCRIPTION_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(
    NAME ${TEST_PENDING_SUBSCRIPTION_NAME}_unsubscribe
COMMAND ${TEST_PENDING_SUBSCRIPTION_ENTRYPOINT}
${NETWORK_TEST_BIN_DIR}/pending_subscription_tests/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER} UNSUBSCRIBE)
set_tests_properties(${TEST_PENDING_SUBSCRIPTION_NAME}_unsubscribe PROPERTIES TIMEOUT 180)

string(REPLACE "test_name" "${TEST_PENDING_SUBSCRIPTION_NAME}_alternating_subscribe_unsubscribe_nack" TEST_PENDING_SUBSCRIPTION_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(
    NAME ${TEST_PENDING_SUBSCRIPTION_NAME}_alternating_subscribe_unsubscribe_nack
COMMAND ${TEST_PENDING_SUBSCRIPTION_ENTRYPOINT}
${NETWORK_TEST_BIN_DIR}/pending_subscription_tests/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER} SUBSCRIBE_UNSUBSCRIBE_NACK)
set_tests_properties(${TEST_PENDING_SUBSCRIPTION_NAME}_alternating_subscribe_unsubscribe_nack PROPERTIES TIMEOUT 180)

string(REPLACE "test_name" "${TEST_PENDING_SUBSCRIPTION_NAME}_alternating_subscribe_unsubscribe_same_port" TEST_PENDING_SUBSCRIPTION_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(
    NAME ${TEST_PENDING_SUBSCRIPTION_NAME}_alternating_subscribe_unsubscribe_same_port
    COMMAND ${TEST_PENDING_SUBSCRIPTION_ENTRYPOINT}
    ${NETWORK_TEST_BIN_DIR}/pending_subscription_tests/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER} SUBSCRIBE_UNSUBSCRIBE_SAME_PORT)
set_tests_properties(${TEST_PENDING_SUBSCRIPTION_NAME}_alternating_subscribe_unsubscribe_same_port PROPERTIES TIMEOUT 180)

string(REPLACE "test_name" "${TEST_PENDING_SUBSCRIPTION_NAME}_subscribe_resubscribe_mixed" TEST_PENDING_SUBSCRIPTION_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(
    NAME ${TEST_PENDING_SUBSCRIPTION_NAME}_subscribe_resubscribe_mixed
    COMMAND ${TEST_PENDING_SUBSCRIPTION_ENTRYPOINT}
    ${NETWORK_TEST_BIN_DIR}/pending_subscription_tests/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER} SUBSCRIBE_RESUBSCRIBE_MIXED)
set_tests_properties(${TEST_PENDING_SUBSCRIPTION_NAME}_subscribe_resubscribe_mixed PROPERTIES TIMEOUT 300)

string(REPLACE "test_name" "${TEST_PENDING_SUBSCRIPTION_NAME}_subscribe_stopsubscribe_subscribe" TEST_PENDING_SUBSCRIPTION_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(
    NAME ${TEST_PENDING_SUBSCRIPTION_NAME}_subscribe_stopsubscribe_subscribe
    COMMAND ${TEST_PENDING_SUBSCRIPTION_ENTRYPOINT}
    ${NETWORK_TEST_BIN_DIR}/pending_subscription_tests/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER} SUBSCRIBE_STOPSUBSCRIBE_SUBSCRIBE)
set_tests_properties(${TEST_PENDING_SUBSCRIPTION_NAME}_subscribe_stopsubscribe_subscribe PROPERTIES TIMEOUT 180)

string(REPLACE "test_name" "${TEST_PENDING_SUBSCRIPTION_NAME}_send_request_to_sd_port" TEST_PENDING_SUBSCRIPTION_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(
    NAME ${TEST_PENDING_SUBSCRIPTION_NAME}_send_request_to_sd_port
    COMMAND ${TEST_PENDING_SUBSCRIPTION_ENTRYPOINT}
    ${NETWORK_TEST_BIN_DIR}/pending_subscription_tests/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER} REQUEST_TO_SD)
set_tests_properties(${TEST_PENDING_SUBSCRIPTION_NAME}_send_request_to_sd_port PROPERTIES TIMEOUT 180)

add_dependencies(${TEST_PENDING_SUBSCRIPTION_SERVICE} gtest)
add_dependencies(${TEST_PENDING_SUBSCRIPTION_CLIENT} gtest)

add_dependencies(build_network_tests ${TEST_PENDING_SUBSCRIPTION_SERVICE})
add_dependencies(build_network_tests ${TEST_PENDING_SUBSCRIPTION_CLIENT})
