# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(debounce_callback_tests LANGUAGES CXX)

set(TEST_DEBOUNCE_CALLBACK_NAME debounce_callback_test)
set(TEST_DEBOUNCE_CALLBACK_CLIENT ${TEST_DEBOUNCE_CALLBACK_NAME}_client)
add_executable(${TEST_DEBOUNCE_CALLBACK_CLIENT} ${TEST_DEBOUNCE_CALLBACK_CLIENT}.cpp)
target_link_libraries(${TEST_DEBOUNCE_CALLBACK_CLIENT}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
)
set(TEST_DEBOUNCE_CALLBACK_SERVICE ${TEST_DEBOUNCE_CALLBACK_NAME}_service)
add_executable(${TEST_DEBOUNCE_CALLBACK_SERVICE} ${TEST_DEBOUNCE_CALLBACK_SERVICE}.cpp)
target_link_libraries(${TEST_DEBOUNCE_CALLBACK_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
)

# copy starter scripts into builddir
set(TEST_DEBOUNCE_CALLBACK_MASTER_STARTER ${TEST_DEBOUNCE_CALLBACK_NAME}_master_starter.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/debounce_callback_tests/${TEST_DEBOUNCE_CALLBACK_MASTER_STARTER}
    ${NETWORK_TEST_BIN_DIR}/debounce_callback_tests/${TEST_DEBOUNCE_CALLBACK_MASTER_STARTER}
    ${TEST_DEBOUNCE_CALLBACK_SERVICE}
)
set(TEST_DEBOUNCE_CALLBACK_SLAVE_STARTER ${TEST_DEBOUNCE_CALLBACK_NAME}_slave_starter.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/debounce_callback_tests/${TEST_DEBOUNCE_CALLBACK_SLAVE_STARTER}
    ${NETWORK_TEST_BIN_DIR}/debounce_callback_tests/${TEST_DEBOUNCE_CALLBACK_SLAVE_STARTER}
    ${TEST_DEBOUNCE_CALLBACK_CLIENT}
)

# configure configurations and copy them into builddir
set(TEST_DEBOUNCE_CALLBACK_MASTER_CONFIG_FILE ${TEST_DEBOUNCE_CALLBACK_SERVICE}.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/debounce_callback_tests/conf/${TEST_DEBOUNCE_CALLBACK_MASTER_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/debounce_callback_tests/${TEST_DEBOUNCE_CALLBACK_MASTER_CONFIG_FILE}
    @ONLY)

set(TEST_DEBOUNCE_CALLBACK_SLAVE_CONFIG_FILE ${TEST_DEBOUNCE_CALLBACK_CLIENT}.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/debounce_callback_tests/conf/${TEST_DEBOUNCE_CALLBACK_SLAVE_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/debounce_callback_tests/${TEST_DEBOUNCE_CALLBACK_SLAVE_CONFIG_FILE}
    @ONLY)

# debounce callback tests
add_test(NAME ${TEST_DEBOUNCE_CALLBACK_NAME}
    COMMAND ${NETWORK_TEST_BIN_DIR}/debounce_callback_tests/${TEST_DEBOUNCE_CALLBACK_MASTER_STARTER})
set_tests_properties(${TEST_DEBOUNCE_CALLBACK_NAME} PROPERTIES TIMEOUT 60)

add_dependencies(${TEST_DEBOUNCE_CALLBACK_CLIENT} gtest)
add_dependencies(${TEST_DEBOUNCE_CALLBACK_SERVICE} gtest)

add_dependencies(build_network_tests ${TEST_DEBOUNCE_CALLBACK_CLIENT})
add_dependencies(build_network_tests ${TEST_DEBOUNCE_CALLBACK_SERVICE})
