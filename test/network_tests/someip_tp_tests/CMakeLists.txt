# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(someip_tp_tests LANGUAGES CXX)

set(TEST_SOMEIPTP_NAME someip_tp_test)
set(TEST_SOMEIPTP_SERVICE ${TEST_SOMEIPTP_NAME}_service)

add_executable(${TEST_SOMEIPTP_SERVICE}
    ${TEST_SOMEIPTP_SERVICE}.cpp)
target_link_libraries(${TEST_SOMEIPTP_SERVICE}
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
set(TEST_SOMEIPTP_CLIENT ${TEST_SOMEIPTP_NAME}_msg_sender)
add_executable(${TEST_SOMEIPTP_CLIENT}
    ${TEST_SOMEIPTP_CLIENT}.cpp
    ${PROJECT_SOURCE_DIR}/../../../implementation/message/src/deserializer.cpp
    ${PROJECT_SOURCE_DIR}/../../../implementation/message/src/message_impl.cpp
    ${PROJECT_SOURCE_DIR}/../../../implementation/message/src/payload_impl.cpp
    ${PROJECT_SOURCE_DIR}/../../../implementation/endpoints/src/tp.cpp
    ${PROJECT_SOURCE_DIR}/../../../implementation/endpoints/src/tp_reassembler.cpp
    ${PROJECT_SOURCE_DIR}/../../../implementation/endpoints/src/tp_message.cpp
    ${sd_sources}
)

target_link_libraries(${TEST_SOMEIPTP_CLIENT}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
    ${VSOMEIP_NAME}
    ${VSOMEIP_NAME}-sd
)

# Copy service config file for external allow tests into $BUILDDIR/test
set(TEST_SOMEIPTP_MASTER_CONFIG_FILE ${TEST_SOMEIPTP_NAME}_master.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/someip_tp_tests/conf/${TEST_SOMEIPTP_MASTER_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/someip_tp_tests/${TEST_SOMEIPTP_MASTER_CONFIG_FILE}
    @ONLY)

# Copy bashscript to start test (master) into $BUILDDIR/test
set(TEST_SOMEIPTP_MASTER_START_SCRIPT ${TEST_SOMEIPTP_NAME}_master_starter.sh)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/someip_tp_tests/conf/${TEST_SOMEIPTP_MASTER_START_SCRIPT}.in
    ${NETWORK_TEST_BIN_DIR}/someip_tp_tests/${TEST_SOMEIPTP_MASTER_START_SCRIPT}
    @ONLY)

# SOME/IP-TP tests
string(REPLACE "test_name" "${TEST_SOMEIPTP_NAME}_in_sequence" TEST_SOMEIPTP_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(NAME ${TEST_SOMEIPTP_NAME}_in_sequence
    COMMAND ${TEST_SOMEIPTP_ENTRYPOINT}
    ${NETWORK_TEST_BIN_DIR}/someip_tp_tests/${TEST_SOMEIPTP_MASTER_START_SCRIPT} IN_SEQUENCE)
set_tests_properties(${TEST_SOMEIPTP_NAME}_in_sequence PROPERTIES TIMEOUT 180)

string(REPLACE "test_name" "${TEST_SOMEIPTP_NAME}_mixed" TEST_SOMEIPTP_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(NAME ${TEST_SOMEIPTP_NAME}_mixed
    COMMAND ${TEST_SOMEIPTP_ENTRYPOINT}
    ${NETWORK_TEST_BIN_DIR}/someip_tp_tests/${TEST_SOMEIPTP_MASTER_START_SCRIPT} MIXED)
set_tests_properties(${TEST_SOMEIPTP_NAME}_mixed PROPERTIES TIMEOUT 180)

string(REPLACE "test_name" "${TEST_SOMEIPTP_NAME}_incomplete" TEST_SOMEIPTP_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(NAME ${TEST_SOMEIPTP_NAME}_incomplete
    COMMAND ${TEST_SOMEIPTP_ENTRYPOINT}
    ${NETWORK_TEST_BIN_DIR}/someip_tp_tests/${TEST_SOMEIPTP_MASTER_START_SCRIPT} INCOMPLETE)
set_tests_properties(${TEST_SOMEIPTP_NAME}_incomplete PROPERTIES TIMEOUT 180)

string(REPLACE "test_name" "${TEST_SOMEIPTP_NAME}_duplicate" TEST_SOMEIPTP_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(NAME ${TEST_SOMEIPTP_NAME}_duplicate
    COMMAND ${TEST_SOMEIPTP_ENTRYPOINT}
    ${NETWORK_TEST_BIN_DIR}/someip_tp_tests/${TEST_SOMEIPTP_MASTER_START_SCRIPT} DUPLICATE)
set_tests_properties(${TEST_SOMEIPTP_NAME}_duplicate PROPERTIES TIMEOUT 180)

string(REPLACE "test_name" "${TEST_SOMEIPTP_NAME}_overlap" TEST_SOMEIPTP_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(NAME ${TEST_SOMEIPTP_NAME}_overlap
    COMMAND ${TEST_SOMEIPTP_ENTRYPOINT}
    ${NETWORK_TEST_BIN_DIR}/someip_tp_tests/${TEST_SOMEIPTP_MASTER_START_SCRIPT} OVERLAP)
set_tests_properties(${TEST_SOMEIPTP_NAME}_overlap PROPERTIES TIMEOUT 180)

string(REPLACE "test_name" "${TEST_SOMEIPTP_NAME}_overlap_front_back" TEST_SOMEIPTP_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(NAME ${TEST_SOMEIPTP_NAME}_overlap_front_back
    COMMAND ${TEST_SOMEIPTP_ENTRYPOINT}
    ${NETWORK_TEST_BIN_DIR}/someip_tp_tests/${TEST_SOMEIPTP_MASTER_START_SCRIPT} OVERLAP_FRONT_BACK)
set_tests_properties(${TEST_SOMEIPTP_NAME}_overlap_front_back PROPERTIES TIMEOUT 180)

add_dependencies(${TEST_SOMEIPTP_CLIENT} gtest)
add_dependencies(${TEST_SOMEIPTP_SERVICE} gtest)

add_dependencies(build_network_tests ${TEST_SOMEIPTP_CLIENT})
add_dependencies(build_network_tests ${TEST_SOMEIPTP_SERVICE})
