# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(offer_tests LANGUAGES CXX)

set(TEST_OFFER_NAME offer_test)
set(TEST_OFFER_SERVICE ${TEST_OFFER_NAME}_service)
add_executable(${TEST_OFFER_SERVICE}
    ${TEST_OFFER_NAME}_service.cpp)
target_link_libraries(${TEST_OFFER_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

set(TEST_OFFER_MULTIPLE_OFFERING offer_test_multiple_offerings)
add_executable(${TEST_OFFER_MULTIPLE_OFFERING}
    ${TEST_OFFER_MULTIPLE_OFFERING}.cpp)
target_link_libraries(${TEST_OFFER_MULTIPLE_OFFERING}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    vsomeip_utilities
)

# copy starter scripts into builddir
set(TEST_OFFER_MULTIPLE_OFFERING_STARTER ${TEST_OFFER_MULTIPLE_OFFERING}_starter.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_MULTIPLE_OFFERING_STARTER}
    ${NETWORK_TEST_BIN_DIR}/offer_tests/${TEST_OFFER_MULTIPLE_OFFERING_STARTER}
    ${TEST_OFFER_MULTIPLE_OFFERING}
)

# Copy config file for local test into $BUILDDIR/test
set(TEST_OFFER_MULTIPLE_OFFERING_CONFIG ${TEST_OFFER_MULTIPLE_OFFERING}.json)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_MULTIPLE_OFFERING_CONFIG}
    ${NETWORK_TEST_BIN_DIR}/offer_tests/${TEST_OFFER_MULTIPLE_OFFERING_CONFIG}
    ${TEST_OFFER_MULTIPLE_OFFERING}
)

set(TEST_OFFER_CLIENT ${TEST_OFFER_NAME}_client)
add_executable(${TEST_OFFER_CLIENT}
    ${TEST_OFFER_NAME}_client.cpp)
target_link_libraries(${TEST_OFFER_CLIENT}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

set(TEST_OFFER_SERVICE_EXTERNAL ${TEST_OFFER_NAME}_service_external)
add_executable(${TEST_OFFER_SERVICE_EXTERNAL}
    ${TEST_OFFER_NAME}_service_external.cpp)
target_link_libraries(${TEST_OFFER_SERVICE_EXTERNAL}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

set(TEST_OFFER_EXTERNAL_SD_MESSAGE_SENDER ${TEST_OFFER_NAME}_external_sd_msg_sender)
add_executable(${TEST_OFFER_EXTERNAL_SD_MESSAGE_SENDER}
    ${TEST_OFFER_NAME}_external_sd_msg_sender.cpp)
target_link_libraries(${TEST_OFFER_EXTERNAL_SD_MESSAGE_SENDER}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

# client and service for offer test big sd msg
set(TEST_OFFER_BIG_NAME offer_test_big_sd_msg)
set(TEST_OFFER_BIG_SERVICE ${TEST_OFFER_BIG_NAME}_service)
add_executable(${TEST_OFFER_BIG_SERVICE}
    ${TEST_OFFER_BIG_NAME}_service.cpp)
target_link_libraries(${TEST_OFFER_BIG_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

set(TEST_OFFER_BIG_CLIENT ${TEST_OFFER_BIG_NAME}_client)
add_executable(${TEST_OFFER_BIG_CLIENT}
    ${TEST_OFFER_BIG_NAME}_client.cpp)
target_link_libraries(${TEST_OFFER_BIG_CLIENT}
    ${DLT_LIBRARIES}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    vsomeip_utilities
)

# copy starter scripts into builddir
if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")
    set(TEST_OFFER_LOCAL_STARTER ${TEST_OFFER_NAME}_local_starter_qnx.sh)
else()
    set(TEST_OFFER_LOCAL_STARTER ${TEST_OFFER_NAME}_local_starter.sh)
endif()

copy_to_builddir(${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_LOCAL_STARTER}
    ${NETWORK_TEST_BIN_DIR}/offer_tests/${TEST_OFFER_LOCAL_STARTER}
    ${TEST_OFFER_SERVICE}
)

# Copy config file for local test into $BUILDDIR/test
set(TEST_OFFER_LOCAL_CONFIG_FILE ${TEST_OFFER_NAME}_local.json)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_LOCAL_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/offer_tests/${TEST_OFFER_LOCAL_CONFIG_FILE}
    ${TEST_OFFER_SERVICE}
)

# generate and copy json files into builddir for external test
set(TEST_OFFER_SLAVE_CONFIG_FILE ${TEST_OFFER_NAME}_external_slave.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/offer_tests/conf/${TEST_OFFER_SLAVE_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/offer_tests/${TEST_OFFER_SLAVE_CONFIG_FILE}
    @ONLY)

set(TEST_OFFER_MASTER_CONFIG_FILE ${TEST_OFFER_NAME}_external_master.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/offer_tests/conf/${TEST_OFFER_MASTER_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/offer_tests/${TEST_OFFER_MASTER_CONFIG_FILE}
    @ONLY)

# generate and copy json files into builddir for big SD message test
set(TEST_OFFER_BIG_SLAVE_CONFIG_FILE ${TEST_OFFER_BIG_NAME}_slave.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/offer_tests/conf/${TEST_OFFER_BIG_SLAVE_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/offer_tests/${TEST_OFFER_BIG_SLAVE_CONFIG_FILE}
    @ONLY)

set(TEST_OFFER_BIG_MASTER_CONFIG_FILE ${TEST_OFFER_BIG_NAME}_master.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/offer_tests/conf/${TEST_OFFER_BIG_MASTER_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/offer_tests/${TEST_OFFER_BIG_MASTER_CONFIG_FILE}
    @ONLY)

# Copy starter scripts for external test to $BUILDDIR/test
if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")
    set(TEST_OFFER_EXTERNAL_MASTER_STARTER ${TEST_OFFER_NAME}_external_master_starter_qnx.sh)
else()
    set(TEST_OFFER_EXTERNAL_MASTER_STARTER ${TEST_OFFER_NAME}_external_master_starter.sh)
endif()

configure_file(
    ${NETWORK_TEST_SRC_DIR}/offer_tests/conf/${TEST_OFFER_EXTERNAL_MASTER_STARTER}.in
    ${NETWORK_TEST_BIN_DIR}/offer_tests/${TEST_OFFER_EXTERNAL_MASTER_STARTER}
    @ONLY)

set(TEST_OFFER_EXTERNAL_SLAVE_STARTER ${TEST_OFFER_NAME}_external_slave_starter.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_EXTERNAL_SLAVE_STARTER}
    ${NETWORK_TEST_BIN_DIR}/offer_tests/${TEST_OFFER_EXTERNAL_SLAVE_STARTER}
    ${TEST_OFFER_SERVICE}
)

# Copy starter scripts for external test to $BUILDDIR/test
set(TEST_OFFER_BIG_MASTER_STARTER ${TEST_OFFER_BIG_NAME}_master_starter.sh)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/offer_tests/conf/${TEST_OFFER_BIG_MASTER_STARTER}.in
    ${NETWORK_TEST_BIN_DIR}/offer_tests/${TEST_OFFER_BIG_MASTER_STARTER}
    @ONLY)

# Copy starter scripts for external test to $BUILDDIR/test
set(TEST_OFFER_BIG_EXTERNAL_SLAVE_STARTER ${TEST_OFFER_BIG_NAME}_slave_starter.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_BIG_EXTERNAL_SLAVE_STARTER}
    ${NETWORK_TEST_BIN_DIR}/offer_tests/${TEST_OFFER_BIG_EXTERNAL_SLAVE_STARTER}
    ${TEST_OFFER_BIG_SERVICE}
)

# offer tests
add_test(NAME ${TEST_OFFER_NAME}_local
    COMMAND ${NETWORK_TEST_BIN_DIR}/offer_tests/${TEST_OFFER_LOCAL_STARTER})
set_tests_properties(${TEST_OFFER_NAME}_local PROPERTIES TIMEOUT 180)
add_test(NAME ${TEST_OFFER_NAME}_external
    COMMAND ${NETWORK_TEST_BIN_DIR}/offer_tests/${TEST_OFFER_EXTERNAL_MASTER_STARTER})
set_tests_properties(${TEST_OFFER_NAME}_local PROPERTIES TIMEOUT 360)

add_test(NAME ${TEST_OFFER_BIG_NAME}
    COMMAND ${NETWORK_TEST_BIN_DIR}/offer_tests/${TEST_OFFER_BIG_MASTER_STARTER})
set_tests_properties(${TEST_OFFER_BIG_NAME} PROPERTIES TIMEOUT 360)

# Multiple Offer tests
add_test(NAME ${TEST_OFFER_MULTIPLE_OFFERING}
    COMMAND ${NETWORK_TEST_BIN_DIR}/offer_tests/${TEST_OFFER_MULTIPLE_OFFERING_STARTER})
set_tests_properties(${TEST_OFFER_MULTIPLE_OFFERING} PROPERTIES TIMEOUT 1800)


add_dependencies(${TEST_OFFER_SERVICE} gtest)
add_dependencies(${TEST_OFFER_CLIENT} gtest)
add_dependencies(${TEST_OFFER_SERVICE_EXTERNAL} gtest)
add_dependencies(${TEST_OFFER_EXTERNAL_SD_MESSAGE_SENDER} gtest)

add_dependencies(build_network_tests ${TEST_OFFER_SERVICE})
add_dependencies(build_network_tests ${TEST_OFFER_CLIENT})
add_dependencies(build_network_tests ${TEST_OFFER_SERVICE_EXTERNAL})
add_dependencies(build_network_tests ${TEST_OFFER_EXTERNAL_SD_MESSAGE_SENDER})
add_dependencies(build_network_tests ${TEST_OFFER_BIG_SERVICE})
add_dependencies(build_network_tests ${TEST_OFFER_BIG_CLIENT})
add_dependencies(build_network_tests ${TEST_OFFER_MULTIPLE_OFFERING})
