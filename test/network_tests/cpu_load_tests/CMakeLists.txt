# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(cpu_load_tests LANGUAGES CXX)

set(TEST_CPU_LOAD_NAME cpu_load_test)

set(TEST_CPU_LOAD_SERVICE cpu_load_test_service)
add_executable(${TEST_CPU_LOAD_SERVICE}
    ${TEST_CPU_LOAD_SERVICE}.cpp
    cpu_load_measurer.cpp
)
target_link_libraries(${TEST_CPU_LOAD_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

# Copy config files for test into $BUILDDIR/test
set(TEST_CPU_LOAD_SERVICE_MASTER_CONFIG_FILE ${TEST_CPU_LOAD_NAME}_service_master.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/cpu_load_tests/conf/${TEST_CPU_LOAD_SERVICE_MASTER_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/cpu_load_tests/${TEST_CPU_LOAD_SERVICE_MASTER_CONFIG_FILE}
    @ONLY)

set(TEST_CPU_LOAD_SERVICE_SLAVE_CONFIG_FILE ${TEST_CPU_LOAD_NAME}_service_slave.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/cpu_load_tests/conf/${TEST_CPU_LOAD_SERVICE_SLAVE_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/cpu_load_tests/${TEST_CPU_LOAD_SERVICE_SLAVE_CONFIG_FILE}
    @ONLY)

##############################################################################
set(TEST_CPU_LOAD_CLIENT cpu_load_test_client)

add_executable(${TEST_CPU_LOAD_CLIENT}
    ${TEST_CPU_LOAD_CLIENT}.cpp
    cpu_load_measurer.cpp
)

target_link_libraries(${TEST_CPU_LOAD_CLIENT}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

set(TEST_CPU_LOAD_CLIENT_MASTER_CONFIG_FILE ${TEST_CPU_LOAD_NAME}_client_master.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/cpu_load_tests/conf/${TEST_CPU_LOAD_CLIENT_MASTER_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/cpu_load_tests/${TEST_CPU_LOAD_CLIENT_MASTER_CONFIG_FILE}
    @ONLY)

set(TEST_CPU_LOAD_CLIENT_SLAVE_CONFIG_FILE ${TEST_CPU_LOAD_NAME}_client_slave.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/cpu_load_tests/conf/${TEST_CPU_LOAD_CLIENT_SLAVE_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/cpu_load_tests/${TEST_CPU_LOAD_CLIENT_SLAVE_CONFIG_FILE}
    @ONLY)

##############################################################################
# copy starter scripts into builddir
set(TEST_CPU_LOAD_MASTER_STARTER ${TEST_CPU_LOAD_NAME}_master_starter.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/cpu_load_tests/${TEST_CPU_LOAD_MASTER_STARTER}
    ${NETWORK_TEST_BIN_DIR}/cpu_load_tests/${TEST_CPU_LOAD_MASTER_STARTER}
    ${TEST_CPU_LOAD_SERVICE}
)
set(TEST_CPU_LOAD_SLAVE_STARTER ${TEST_CPU_LOAD_NAME}_slave_starter.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/cpu_load_tests/${TEST_CPU_LOAD_SLAVE_STARTER}
    ${NETWORK_TEST_BIN_DIR}/cpu_load_tests/${TEST_CPU_LOAD_SLAVE_STARTER}
    ${TEST_CPU_LOAD_SERVICE}
)

# cpu load tests
add_test(NAME ${TEST_CPU_LOAD_NAME}
    COMMAND ${NETWORK_TEST_BIN_DIR}/cpu_load_tests/${TEST_CPU_LOAD_MASTER_STARTER}
)
set_tests_properties(${TEST_CPU_LOAD_NAME} PROPERTIES TIMEOUT 3000)

add_dependencies(${TEST_CPU_LOAD_SERVICE} gtest)
add_dependencies(${TEST_CPU_LOAD_CLIENT} gtest)

add_dependencies(build_network_tests ${TEST_CPU_LOAD_SERVICE})
add_dependencies(build_network_tests ${TEST_CPU_LOAD_CLIENT})
