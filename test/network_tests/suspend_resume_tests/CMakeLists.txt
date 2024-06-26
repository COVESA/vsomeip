# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(suspend_resume_tests LANGUAGES CXX)

set(TEST_SUSPEND_RESUME_NAME suspend_resume_test)
set(TEST_SUSPEND_RESUME_SERVICE ${TEST_SUSPEND_RESUME_NAME}_service)
set(TEST_SUSPEND_RESUME_CLIENT ${TEST_SUSPEND_RESUME_NAME}_client)

add_executable(${TEST_SUSPEND_RESUME_SERVICE}
    ${TEST_SUSPEND_RESUME_SERVICE}.cpp
)
target_link_libraries(${TEST_SUSPEND_RESUME_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

add_executable(${TEST_SUSPEND_RESUME_CLIENT}
    ${TEST_SUSPEND_RESUME_CLIENT}.cpp
)
target_link_libraries(${TEST_SUSPEND_RESUME_CLIENT}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

set(TEST_SUSPEND_RESUME_SERVICE_CONFIG_FILE ${TEST_SUSPEND_RESUME_SERVICE}.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/suspend_resume_tests/conf/${TEST_SUSPEND_RESUME_SERVICE_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/suspend_resume_tests/${TEST_SUSPEND_RESUME_SERVICE_CONFIG_FILE}
    @ONLY)

set(TEST_SUSPEND_RESUME_CLIENT_CONFIG_FILE ${TEST_SUSPEND_RESUME_CLIENT}.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/suspend_resume_tests/conf/${TEST_SUSPEND_RESUME_CLIENT_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/suspend_resume_tests/${TEST_SUSPEND_RESUME_CLIENT_CONFIG_FILE}
    @ONLY)

set(TEST_SUSPEND_RESUME_MASTER_START_SCRIPT ${TEST_SUSPEND_RESUME_NAME}_master_starter.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/suspend_resume_tests/${TEST_SUSPEND_RESUME_MASTER_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/suspend_resume_tests/${TEST_SUSPEND_RESUME_MASTER_START_SCRIPT}
    ${TEST_SUSPEND_RESUME_CLIENT}
)

set(TEST_SUSPEND_RESUME_SLAVE_START_SCRIPT ${TEST_SUSPEND_RESUME_NAME}_slave_starter.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/suspend_resume_tests/${TEST_SUSPEND_RESUME_SLAVE_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/suspend_resume_tests/${TEST_SUSPEND_RESUME_SLAVE_START_SCRIPT}
    ${TEST_SUSPEND_RESUME_CLIENT}
)

add_test(NAME ${TEST_SUSPEND_RESUME_NAME}_initial
    COMMAND ${NETWORK_TEST_BIN_DIR}/suspend_resume_tests/${TEST_SUSPEND_RESUME_MASTER_START_SCRIPT} SERVICE
)

add_dependencies(${TEST_SUSPEND_RESUME_CLIENT} gtest)
add_dependencies(${TEST_SUSPEND_RESUME_SERVICE} gtest)

add_dependencies(build_network_tests ${TEST_SUSPEND_RESUME_CLIENT})
add_dependencies(build_network_tests ${TEST_SUSPEND_RESUME_SERVICE})
