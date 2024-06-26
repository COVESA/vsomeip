# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(security_tests LANGUAGES CXX)

if (${TEST_SECURITY})
    set(TEST_SECURITY_NAME security_test)
    set(TEST_SECURITY_SERVICE security_test_service)
    set(TEST_SECURITY_CLIENT security_test_client)

    add_executable(${TEST_SECURITY_SERVICE}
        ${TEST_SECURITY_SERVICE}.cpp)
    target_link_libraries(${TEST_SECURITY_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    vsomeip_utilities
        ${DLT_LIBRARIES}
    )

    # Copy config file for service into $BUILDDIR/test
    set(TEST_SECURITY_LOCAL_CONFIG_FILE ${TEST_SECURITY_NAME}_local_config.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/security_tests/conf/${TEST_SECURITY_LOCAL_CONFIG_FILE}.in
        ${NETWORK_TEST_BIN_DIR}/security_tests/${TEST_SECURITY_LOCAL_CONFIG_FILE}
        @ONLY)

    # Copy service config file for external allow tests into $BUILDDIR/test
    set(TEST_SECURITY_SERVICE_CONFIG_FILE_EXTERNAL ${TEST_SECURITY_NAME}_config_service_external_allow.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/security_tests/conf/${TEST_SECURITY_SERVICE_CONFIG_FILE_EXTERNAL}.in
        ${NETWORK_TEST_BIN_DIR}/security_tests/${TEST_SECURITY_SERVICE_CONFIG_FILE_EXTERNAL}
        @ONLY)

    # Copy client config file for external allow tests into $BUILDDIR/test
    set(TEST_SECURITY_CLIENT_CONFIG_FILE_EXTERNAL ${TEST_SECURITY_NAME}_config_client_external_allow.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/security_tests/conf/${TEST_SECURITY_CLIENT_CONFIG_FILE_EXTERNAL}.in
        ${NETWORK_TEST_BIN_DIR}/security_tests/${TEST_SECURITY_CLIENT_CONFIG_FILE_EXTERNAL}
        @ONLY)

    # Copy service config file for external deny tests into $BUILDDIR/test
    set(TEST_SECURITY_SERVICE_CONFIG_FILE_EXTERNAL_DENY ${TEST_SECURITY_NAME}_config_service_external_deny.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/security_tests/conf/${TEST_SECURITY_SERVICE_CONFIG_FILE_EXTERNAL_DENY}.in
        ${NETWORK_TEST_BIN_DIR}/security_tests/${TEST_SECURITY_SERVICE_CONFIG_FILE_EXTERNAL_DENY}
        @ONLY)

    # Copy client config file for external deny tests into $BUILDDIR/test
    set(TEST_SECURITY_CLIENT_CONFIG_FILE_EXTERNAL_DENY ${TEST_SECURITY_NAME}_config_client_external_deny.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/security_tests/conf/${TEST_SECURITY_CLIENT_CONFIG_FILE_EXTERNAL_DENY}.in
        ${NETWORK_TEST_BIN_DIR}/security_tests/${TEST_SECURITY_CLIENT_CONFIG_FILE_EXTERNAL_DENY}
        @ONLY)

    # Copy bashscript to start local test into $BUILDDIR/test
    set(TEST_SECURITY_SERVICE_LOCAL_START_SCRIPT ${TEST_SECURITY_NAME}_local_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/security_tests/${TEST_SECURITY_SERVICE_LOCAL_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/security_tests/${TEST_SECURITY_SERVICE_LOCAL_START_SCRIPT}
        ${TEST_SECURITY_SERVICE}
    )

    # Copy bashscript to start external tests (master) into $BUILDDIR/test
    set(TEST_SECURITY_EXTERNAL_MASTER_START_SCRIPT ${TEST_SECURITY_NAME}_external_master_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/security_tests/${TEST_SECURITY_EXTERNAL_MASTER_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/security_tests/${TEST_SECURITY_EXTERNAL_MASTER_START_SCRIPT}
        ${TEST_SECURITY_SERVICE}
    )

    # Copy bashscript to start external tests (slave) into $BUILDDIR/test
    set(TEST_SECURITY_EXTERNAL_SLAVE_START_SCRIPT ${TEST_SECURITY_NAME}_external_slave_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/security_tests/${TEST_SECURITY_EXTERNAL_SLAVE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/security_tests/${TEST_SECURITY_EXTERNAL_SLAVE_START_SCRIPT}
        ${TEST_SECURITY_SERVICE}
    )

    add_executable(${TEST_SECURITY_CLIENT}
        ${TEST_SECURITY_CLIENT}.cpp
    )
    target_link_libraries(${TEST_SECURITY_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    vsomeip_utilities
        ${DLT_LIBRARIES}
    )

    # Security tests
    string(REPLACE "test_name" "${TEST_SECURITY_NAME}" TEST_SECURITY_ENTRYPOINT "${TEST_ENTRYPOINT}")
    add_test(NAME ${TEST_SECURITY_NAME}
        COMMAND ${TEST_SECURITY_ENTRYPOINT}
        ${NETWORK_TEST_BIN_DIR}/security_tests/${TEST_SECURITY_SERVICE_LOCAL_START_SCRIPT}
    )
    string(REPLACE "test_name" "${TEST_SECURITY_NAME}_external_allow" TEST_SECURITY_EXTERNAL_ENTRYPOINT "${TEST_ENTRYPOINT}")
    add_test(NAME ${TEST_SECURITY_NAME}_external_allow
        COMMAND ${TEST_SECURITY_EXTERNAL_ENTRYPOINT}
        ${NETWORK_TEST_BIN_DIR}/security_tests/${TEST_SECURITY_EXTERNAL_MASTER_START_SCRIPT} security_test_config_client_external_allow.json --allow
    )
    string(REPLACE "test_name" "${TEST_SECURITY_NAME}_external_deny" TEST_SECURITY_EXTERNAL_ENTRYPOINT "${TEST_ENTRYPOINT}")
    add_test(NAME ${TEST_SECURITY_NAME}_external_deny
        COMMAND ${TEST_SECURITY_EXTERNAL_ENTRYPOINT}
        ${NETWORK_TEST_BIN_DIR}/security_tests/${TEST_SECURITY_EXTERNAL_MASTER_START_SCRIPT} security_test_config_client_external_deny.json --deny
    )

    add_dependencies(${TEST_SECURITY_SERVICE} gtest)
    add_dependencies(${TEST_SECURITY_CLIENT} gtest)

    add_dependencies(build_network_tests ${TEST_SECURITY_SERVICE})
    add_dependencies(build_network_tests ${TEST_SECURITY_CLIENT})
endif()
