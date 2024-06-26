# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(lazy_load_tests LANGUAGES CXX)

if (${TEST_SECURITY})
    set(LAZY_LOAD_NAME lazy_load_test)
    set(LAZY_LOAD_SERVICE lazy_load_test_service)
    set(LAZY_LOAD_CLIENT lazy_load_test_client)
    set(LAZY_LOAD_LAZY_CLIENT lazy_load_test_lazy_client)

    add_executable(${LAZY_LOAD_SERVICE}
        ${LAZY_LOAD_SERVICE}.cpp)
    target_link_libraries(${LAZY_LOAD_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
    )

    # Copy main config file into $BUILDDIR/test and $BUILDDIR/test/vsomeip
    set(LAZY_LOAD_CONFIG_FILE ${LAZY_LOAD_NAME}_config.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/lazy_load_tests/conf/${LAZY_LOAD_CONFIG_FILE}.in
        ${NETWORK_TEST_BIN_DIR}/lazy_load_tests/${LAZY_LOAD_CONFIG_FILE}
        @ONLY)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/lazy_load_tests/conf/${LAZY_LOAD_CONFIG_FILE}.in
        ${NETWORK_TEST_BIN_DIR}/lazy_load_tests/vsomeip/${LAZY_LOAD_CONFIG_FILE}
        @ONLY)

    # Copy vsomeip policy extensions file for tests into $BUILDDIR/test
    set(LAZY_LOAD_POLICY_EXTENSIONS vsomeip_policy_extensions.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/lazy_load_tests/conf/${LAZY_LOAD_POLICY_EXTENSIONS}.in
        ${NETWORK_TEST_BIN_DIR}/lazy_load_tests/vsomeip/${LAZY_LOAD_POLICY_EXTENSIONS}
        @ONLY)

    # Copy vsomeip security extension to into $BUILDDIR/test
    set(LAZY_LOAD_VSOMEIP_SECURITY vsomeip_security.json)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/lazy_load_tests/extensions/1_1/${LAZY_LOAD_VSOMEIP_SECURITY}
        ${NETWORK_TEST_BIN_DIR}/lazy_load_tests/vsomeip/vsomeip_ext/1_1/${LAZY_LOAD_VSOMEIP_SECURITY}
        ${LAZY_LOAD_SERVICE}
    )

    # Copy bashscript to start local test into $BUILDDIR/test
    set(LAZY_LOAD_SERVICE_START_SCRIPT ${LAZY_LOAD_NAME}_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/lazy_load_tests/${LAZY_LOAD_SERVICE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/lazy_load_tests/${LAZY_LOAD_SERVICE_START_SCRIPT}
        ${LAZY_LOAD_SERVICE}
    )

    add_executable(${LAZY_LOAD_CLIENT}
        ${LAZY_LOAD_CLIENT}.cpp
    )
    target_link_libraries(${LAZY_LOAD_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
    )

    add_executable(${LAZY_LOAD_LAZY_CLIENT}
        ${LAZY_LOAD_LAZY_CLIENT}.cpp
    )
    target_link_libraries(${LAZY_LOAD_LAZY_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
    )

    add_test(NAME ${LAZY_LOAD_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/lazy_load_tests/${LAZY_LOAD_SERVICE_START_SCRIPT}
    )

    add_dependencies(${LAZY_LOAD_SERVICE} gtest)
    add_dependencies(${LAZY_LOAD_CLIENT} gtest)
    add_dependencies(${LAZY_LOAD_LAZY_CLIENT} gtest)

    add_dependencies(build_network_tests ${LAZY_LOAD_SERVICE})
    add_dependencies(build_network_tests ${LAZY_LOAD_CLIENT})
    add_dependencies(build_network_tests ${LAZY_LOAD_LAZY_CLIENT})
endif()
