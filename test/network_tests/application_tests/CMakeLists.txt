# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(application_tests LANGUAGES CXX)

set(TEST_APPLICATION application_test)

add_executable(${TEST_APPLICATION} ${TEST_APPLICATION}.cpp)
target_link_libraries(${TEST_APPLICATION}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

if(NOT ${TESTS_BAT})
    set(TEST_APPLICATION_SINGLE_PROCESS_NAME ${TEST_APPLICATION}_single_process)
    add_executable(${TEST_APPLICATION_SINGLE_PROCESS_NAME} ${TEST_APPLICATION_SINGLE_PROCESS_NAME}.cpp)
    target_link_libraries(${TEST_APPLICATION_SINGLE_PROCESS_NAME}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )
    if(QNX)
        target_compile_definitions(${TEST_APPLICATION_SINGLE_PROCESS_NAME} PRIVATE _QNX_SOURCE)
    endif()

    set(TEST_APPLICATION_AVAILABILITY_NAME ${TEST_APPLICATION}_availability)
    add_executable(${TEST_APPLICATION_AVAILABILITY_NAME} ${TEST_APPLICATION_AVAILABILITY_NAME}.cpp)
    target_link_libraries(${TEST_APPLICATION_AVAILABILITY_NAME}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_APPLICATION_SINGLE_PROCESS_CONFIGURATION_FILE ${TEST_APPLICATION}_single_process.json)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/application_tests/${TEST_APPLICATION_SINGLE_PROCESS_CONFIGURATION_FILE}
        ${NETWORK_TEST_BIN_DIR}/application_tests/${TEST_APPLICATION_SINGLE_PROCESS_CONFIGURATION_FILE}
        ${TEST_APPLICATION}_single_process
    )

    set(TEST_APPLICATION_SINGLE_PROCESS_STARTER ${TEST_APPLICATION}_single_process_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/application_tests/${TEST_APPLICATION_SINGLE_PROCESS_STARTER}
        ${NETWORK_TEST_BIN_DIR}/application_tests/${TEST_APPLICATION_SINGLE_PROCESS_STARTER}
        ${TEST_APPLICATION}_single_process
    )

    set(TEST_APPLICATION_AVAILABILITY_STARTER ${TEST_APPLICATION_AVAILABILITY_NAME}_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/application_tests/${TEST_APPLICATION_AVAILABILITY_STARTER}
        ${NETWORK_TEST_BIN_DIR}/application_tests/${TEST_APPLICATION_AVAILABILITY_STARTER}
        ${TEST_APPLICATION_SINGLE_PROCESS_NAME}
    )

endif()

set(TEST_APPLICATION_CONFIGURATION_FILE ${TEST_APPLICATION}.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/application_tests/conf/${TEST_APPLICATION_CONFIGURATION_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/application_tests/${TEST_APPLICATION_CONFIGURATION_FILE}
    @ONLY)

set(TEST_APPLICATION_CONFIGURATION_FILE_DAEMON ${TEST_APPLICATION}_daemon.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/application_tests/conf/${TEST_APPLICATION_CONFIGURATION_FILE_DAEMON}.in
    ${NETWORK_TEST_BIN_DIR}/application_tests/${TEST_APPLICATION_CONFIGURATION_FILE_DAEMON}
    @ONLY)

set(TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE ${TEST_APPLICATION}_no_dispatch_threads.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/application_tests/conf/${TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE}.in
    ${NETWORK_TEST_BIN_DIR}/application_tests/${TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE}
    @ONLY)

set(TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE_DAEMON ${TEST_APPLICATION}_no_dispatch_threads_daemon.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/application_tests/conf/${TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE_DAEMON}.in
    ${NETWORK_TEST_BIN_DIR}/application_tests/${TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE_DAEMON}
    @ONLY)

set(TEST_APPLICATION_STARTER ${TEST_APPLICATION}_starter.sh)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/application_tests/conf/${TEST_APPLICATION_STARTER}.bat.in
    ${NETWORK_TEST_BIN_DIR}/application_tests/${TEST_APPLICATION_STARTER}
    @ONLY)

string(REPLACE "test_name" "${TEST_APPLICATION}" TEST_APPLICATION_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(NAME ${TEST_APPLICATION}
    COMMAND ${TEST_APPLICATION_ENTRYPOINT} ${NETWORK_TEST_BIN_DIR}/application_tests/${TEST_APPLICATION_STARTER}
)
set_tests_properties(${TEST_APPLICATION} PROPERTIES TIMEOUT 80)

add_dependencies(${TEST_APPLICATION} gtest)

add_dependencies(build_network_tests ${TEST_APPLICATION})
