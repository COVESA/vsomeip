# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(debounce_frequency_test LANGUAGES CXX)

set(TEST_DEBOUNCE_FREQUENCY_NAME debounce_frequency_test)
set(TEST_DEBOUNCE_FREQUENCY_CLIENT ${TEST_DEBOUNCE_FREQUENCY_NAME}_client)
add_executable(${TEST_DEBOUNCE_FREQUENCY_CLIENT} ${TEST_DEBOUNCE_FREQUENCY_CLIENT}.cpp)
target_link_libraries(${TEST_DEBOUNCE_FREQUENCY_CLIENT}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    vsomeip_utilities
)
set(TEST_DEBOUNCE_FREQUENCY_SERVICE ${TEST_DEBOUNCE_FREQUENCY_NAME}_service)
add_executable(${TEST_DEBOUNCE_FREQUENCY_SERVICE} ${TEST_DEBOUNCE_FREQUENCY_SERVICE}.cpp)
target_link_libraries(${TEST_DEBOUNCE_FREQUENCY_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    vsomeip_utilities
)

# copy starter scripts into builddir
set(TEST_DEBOUNCE_FREQUENCY_MASTER_STARTER ${TEST_DEBOUNCE_FREQUENCY_NAME}_master_starter.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/debounce_frequency_tests/${TEST_DEBOUNCE_FREQUENCY_MASTER_STARTER}
    ${NETWORK_TEST_BIN_DIR}/debounce_frequency_tests/${TEST_DEBOUNCE_FREQUENCY_MASTER_STARTER}
    ${TEST_DEBOUNCE_FREQUENCY_CLIENT}
)
set(TEST_DEBOUNCE_FREQUENCY_SLAVE_STARTER ${TEST_DEBOUNCE_FREQUENCY_NAME}_slave_starter.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/debounce_frequency_tests/${TEST_DEBOUNCE_FREQUENCY_SLAVE_STARTER}
    ${NETWORK_TEST_BIN_DIR}/debounce_frequency_tests/${TEST_DEBOUNCE_FREQUENCY_SLAVE_STARTER}
    ${TEST_DEBOUNCE_FREQUENCY_SERVICE}
)

# configure configurations and copy them into builddir
set(TEST_DEBOUNCE_FREQUENCY_MASTER_CONFIG_FILE ${TEST_DEBOUNCE_FREQUENCY_CLIENT}.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/debounce_frequency_tests/conf/${TEST_DEBOUNCE_FREQUENCY_MASTER_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/debounce_frequency_tests/${TEST_DEBOUNCE_FREQUENCY_MASTER_CONFIG_FILE}
    @ONLY)

set(TEST_DEBOUNCE_FREQUENCY_SLAVE_CONFIG_FILE ${TEST_DEBOUNCE_FREQUENCY_SERVICE}.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/debounce_frequency_tests/conf/${TEST_DEBOUNCE_FREQUENCY_SLAVE_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/debounce_frequency_tests/${TEST_DEBOUNCE_FREQUENCY_SLAVE_CONFIG_FILE}
    @ONLY)

# debounce frequency tests
add_test(NAME ${TEST_DEBOUNCE_FREQUENCY_NAME}
COMMAND ${NETWORK_TEST_BIN_DIR}/debounce_frequency_tests/${TEST_DEBOUNCE_FREQUENCY_MASTER_STARTER})
set_tests_properties(${TEST_DEBOUNCE_FREQUENCY_NAME} PROPERTIES TIMEOUT 30)

add_dependencies(${TEST_DEBOUNCE_FREQUENCY_CLIENT} gtest)
add_dependencies(${TEST_DEBOUNCE_FREQUENCY_SERVICE} gtest)

add_dependencies(build_network_tests ${TEST_DEBOUNCE_FREQUENCY_CLIENT})
add_dependencies(build_network_tests ${TEST_DEBOUNCE_FREQUENCY_SERVICE})
