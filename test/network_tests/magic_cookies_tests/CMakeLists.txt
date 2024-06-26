# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(magic_cookies_tests LANGUAGES CXX)

set(TEST_MAGIC_COOKIES_NAME magic_cookies_test)
set(TEST_MAGIC_COOKIES_CLIENT ${TEST_MAGIC_COOKIES_NAME}_client)
add_executable(${TEST_MAGIC_COOKIES_CLIENT}
    ${TEST_MAGIC_COOKIES_CLIENT}.cpp)
target_link_libraries(${TEST_MAGIC_COOKIES_CLIENT}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

set(TEST_MAGIC_COOKIES_SERVICE ${TEST_MAGIC_COOKIES_NAME}_service)
add_executable(${TEST_MAGIC_COOKIES_SERVICE}
    ${TEST_MAGIC_COOKIES_SERVICE}.cpp)
target_link_libraries(${TEST_MAGIC_COOKIES_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${DLT_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${TEST_LINK_LIBRARIES}
    vsomeip_utilities
)

# Copy config file for client into $BUILDDIR/test
set(TEST_MAGIC_COOKIES_CLIENT_CONFIG_FILE ${TEST_MAGIC_COOKIES_CLIENT}.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/magic_cookies_tests/conf/${TEST_MAGIC_COOKIES_CLIENT_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/magic_cookies_tests/${TEST_MAGIC_COOKIES_CLIENT_CONFIG_FILE}
    @ONLY)

# Copy bashscript to start client into $BUILDDIR/test
set(TEST_MAGIC_COOKIES_CLIENT_START_SCRIPT ${TEST_MAGIC_COOKIES_CLIENT}_start.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/magic_cookies_tests/${TEST_MAGIC_COOKIES_CLIENT_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/magic_cookies_tests/${TEST_MAGIC_COOKIES_CLIENT_START_SCRIPT}
    ${TEST_MAGIC_COOKIES_CLIENT}
)

set(TEST_MAGIC_COOKIES_SERVICE magic_cookies_test_service)
# Copy config file for service into $BUILDDIR/test
set(TEST_MAGIC_COOKIES_SERVICE_CONFIG_FILE ${TEST_MAGIC_COOKIES_SERVICE}.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/magic_cookies_tests/conf/${TEST_MAGIC_COOKIES_SERVICE_CONFIG_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/magic_cookies_tests/${TEST_MAGIC_COOKIES_SERVICE_CONFIG_FILE}
    @ONLY)

# Copy bashscript to start service into $BUILDDIR/test
set(TEST_MAGIC_COOKIES_SERVICE_START_SCRIPT ${TEST_MAGIC_COOKIES_SERVICE}_start.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/magic_cookies_tests/${TEST_MAGIC_COOKIES_SERVICE_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/magic_cookies_tests/${TEST_MAGIC_COOKIES_SERVICE_START_SCRIPT}
    ${TEST_MAGIC_COOKIES_CLIENT}
)

# Copy bashscript to start client and server $BUILDDIR/test
set(TEST_MAGIC_COOKIES_STARTER ${TEST_MAGIC_COOKIES_NAME}_starter.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/magic_cookies_tests/${TEST_MAGIC_COOKIES_STARTER}
    ${NETWORK_TEST_BIN_DIR}/magic_cookies_tests/${TEST_MAGIC_COOKIES_STARTER}
    ${TEST_MAGIC_COOKIES_CLIENT}
)

# magic cookies test
string(REPLACE "test_name" "${TEST_MAGIC_COOKIES_NAME}" TEST_MAGIC_COOKIES_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(NAME ${TEST_MAGIC_COOKIES_NAME}
    COMMAND ${TEST_MAGIC_COOKIES_ENTRYPOINT} ${NETWORK_TEST_BIN_DIR}/magic_cookies_tests/${TEST_MAGIC_COOKIES_STARTER}
)
set_tests_properties(${TEST_MAGIC_COOKIES_NAME} PROPERTIES TIMEOUT 250)

add_dependencies(${TEST_MAGIC_COOKIES_CLIENT} gtest)
add_dependencies(${TEST_MAGIC_COOKIES_SERVICE} gtest)

add_dependencies(build_network_tests ${TEST_MAGIC_COOKIES_CLIENT})
add_dependencies(build_network_tests ${TEST_MAGIC_COOKIES_SERVICE})
