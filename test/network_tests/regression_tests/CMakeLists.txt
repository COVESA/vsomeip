# Copyright (C) 2024 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(regression_tests LANGUAGES CXX)

# climate_test
set(TEST_CLIMATE_NAME climate_test)

# service
set(TEST_CLIMATE_SERVICE ${TEST_CLIMATE_NAME}_service)
add_executable(${TEST_CLIMATE_SERVICE}
    ${TEST_CLIMATE_SERVICE}.cpp)
target_link_libraries(${TEST_CLIMATE_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)
# client
set(TEST_CLIMATE_CLIENT ${TEST_CLIMATE_NAME}_client)
add_executable(${TEST_CLIMATE_CLIENT}
    ${TEST_CLIMATE_CLIENT}.cpp)
target_link_libraries(${TEST_CLIMATE_CLIENT}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

# copy starter scripts into builddir
set(TEST_CLIMATE_MASTER_STARTER ${TEST_CLIMATE_NAME}_master_starter.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/regression_tests/${TEST_CLIMATE_MASTER_STARTER}
    ${NETWORK_TEST_BIN_DIR}/regression_tests/${TEST_CLIMATE_MASTER_STARTER}
    ${TEST_CLIMATE_SERVICE}
)
set(TEST_CLIMATE_SLAVE_STARTER ${TEST_CLIMATE_NAME}_slave_starter.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/regression_tests/${TEST_CLIMATE_SLAVE_STARTER}
    ${NETWORK_TEST_BIN_DIR}/regression_tests/${TEST_CLIMATE_SLAVE_STARTER}
    ${TEST_CLIMATE_SERVICE}
)

set(TEST_CLIMATE_CONFIG_FILE
    ${TEST_CLIMATE_NAME}_master.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/regression_tests/conf/${TEST_CLIMATE_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/regression_tests/${TEST_CLIMATE_CONFIG_FILE}
    @ONLY)

set(TEST_CLIMATE_CONFIG_FILE
    ${TEST_CLIMATE_NAME}_slave.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/regression_tests/conf/${TEST_CLIMATE_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/regression_tests/${TEST_CLIMATE_CONFIG_FILE}
    @ONLY)

add_dependencies(${TEST_CLIMATE_SERVICE} gtest)
add_dependencies(${TEST_CLIMATE_CLIENT} gtest)

add_dependencies(build_network_tests ${TEST_CLIMATE_SERVICE})
add_dependencies(build_network_tests ${TEST_CLIMATE_CLIENT})

add_test(NAME ${TEST_CLIMATE_NAME}
    COMMAND ${NETWORK_TEST_BIN_DIR}/regression_tests/${TEST_CLIMATE_MASTER_STARTER})
set_tests_properties(${TEST_CLIMATE_NAME} PROPERTIES TIMEOUT 25)
