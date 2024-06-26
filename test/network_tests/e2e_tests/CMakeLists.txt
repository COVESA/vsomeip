# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(e2e_tests LANGUAGES CXX)


set(TEST_E2E_NAME e2e_test)
set(TEST_E2E_SERVICE e2e_test_service)
set(TEST_E2E_CLIENT e2e_test_client)

add_executable(${TEST_E2E_SERVICE} ${TEST_E2E_SERVICE}.cpp)
target_link_libraries(${TEST_E2E_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

add_executable(${TEST_E2E_CLIENT}
    ${TEST_E2E_CLIENT}.cpp
)
target_link_libraries(${TEST_E2E_CLIENT}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

# Copy service config file for external allow tests into $BUILDDIR/test
set(TEST_E2E_SERVICE_CONFIG_FILE_EXTERNAL ${TEST_E2E_NAME}_service_external.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/e2e_tests/conf/${TEST_E2E_SERVICE_CONFIG_FILE_EXTERNAL}.in
    ${NETWORK_TEST_BIN_DIR}/e2e_tests/${TEST_E2E_SERVICE_CONFIG_FILE_EXTERNAL}
    @ONLY)

# Copy client config file for external allow tests into $BUILDDIR/test
set(TEST_E2E_CLIENT_CONFIG_FILE_EXTERNAL ${TEST_E2E_NAME}_client_external.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/e2e_tests/conf/${TEST_E2E_CLIENT_CONFIG_FILE_EXTERNAL}.in
    ${NETWORK_TEST_BIN_DIR}/e2e_tests/${TEST_E2E_CLIENT_CONFIG_FILE_EXTERNAL}
    @ONLY)

# Copy bashscript to start external tests (master) into $BUILDDIR/test
set(TEST_E2E_EXTERNAL_MASTER_START_SCRIPT ${TEST_E2E_NAME}_external_master_start.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_EXTERNAL_MASTER_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/e2e_tests/${TEST_E2E_EXTERNAL_MASTER_START_SCRIPT}
    ${TEST_E2E_SERVICE}
)

# Copy bashscript to start external tests (slave) into $BUILDDIR/test
set(TEST_E2E_EXTERNAL_SLAVE_START_SCRIPT ${TEST_E2E_NAME}_external_slave_start.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_EXTERNAL_SLAVE_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/e2e_tests/${TEST_E2E_EXTERNAL_SLAVE_START_SCRIPT}
    ${TEST_E2E_SERVICE}
)

add_test(
    NAME ${TEST_E2E_NAME}_external
    COMMAND ${NETWORK_TEST_BIN_DIR}/e2e_tests/${TEST_E2E_EXTERNAL_MASTER_START_SCRIPT} e2e_test_client_external.json)
set_tests_properties(${TEST_E2E_NAME}_external PROPERTIES TIMEOUT 180)

add_dependencies(${TEST_E2E_SERVICE} gtest)
add_dependencies(${TEST_E2E_CLIENT} gtest)

add_dependencies(build_network_tests ${TEST_E2E_SERVICE})
add_dependencies(build_network_tests ${TEST_E2E_CLIENT})

##############################################################################
# e2e profile 04 test
##############################################################################

if(${TEST_E2E_PROFILE_04})
    set(TEST_E2E_PROFILE_04_NAME e2e_profile_04_test)
    set(TEST_E2E_PROFILE_04_SERVICE e2e_profile_04_test_service)
    set(TEST_E2E_PROFILE_04_CLIENT e2e_profile_04_test_client)

    add_executable(${TEST_E2E_PROFILE_04_SERVICE} ${TEST_E2E_PROFILE_04_SERVICE}.cpp)
    target_link_libraries(${TEST_E2E_PROFILE_04_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        vsomeip_utilities
        ${DLT_LIBRARIES}
    )

    add_executable(${TEST_E2E_PROFILE_04_CLIENT}
        ${TEST_E2E_PROFILE_04_CLIENT}.cpp
    )
    target_link_libraries(${TEST_E2E_PROFILE_04_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        vsomeip_utilities
        ${DLT_LIBRARIES}
    )

    # Copy service config file for external allow tests into $BUILDDIR/test
    set(TEST_E2E_PROFILE_04_SERVICE_CONFIG_FILE_EXTERNAL ${TEST_E2E_PROFILE_04_NAME}_service_external.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/conf/${TEST_E2E_PROFILE_04_SERVICE_CONFIG_FILE_EXTERNAL}.in
        ${NETWORK_TEST_BIN_DIR}/e2e_tests/${TEST_E2E_PROFILE_04_SERVICE_CONFIG_FILE_EXTERNAL}
        @ONLY)

    # Copy client config file for external allow tests into $BUILDDIR/test
    set(TEST_E2E_PROFILE_04_CLIENT_CONFIG_FILE_EXTERNAL ${TEST_E2E_PROFILE_04_NAME}_client_external.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/conf/${TEST_E2E_PROFILE_04_CLIENT_CONFIG_FILE_EXTERNAL}.in
        ${NETWORK_TEST_BIN_DIR}/e2e_tests/${TEST_E2E_PROFILE_04_CLIENT_CONFIG_FILE_EXTERNAL}
        @ONLY)

    # Copy bashscript to start external tests (master) into $BUILDDIR/test
    set(TEST_E2E_PROFILE_04_EXTERNAL_MASTER_START_SCRIPT ${TEST_E2E_PROFILE_04_NAME}_external_master_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_PROFILE_04_EXTERNAL_MASTER_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/e2e_tests/${TEST_E2E_PROFILE_04_EXTERNAL_MASTER_START_SCRIPT}
        ${TEST_E2E_PROFILE_04_SERVICE}
    )

    # Copy bashscript to start external tests (slave) into $BUILDDIR/test
    set(TEST_E2E_PROFILE_04_EXTERNAL_SLAVE_START_SCRIPT ${TEST_E2E_PROFILE_04_NAME}_external_slave_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_PROFILE_04_EXTERNAL_SLAVE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/e2e_tests/${TEST_E2E_PROFILE_04_EXTERNAL_SLAVE_START_SCRIPT}
        ${TEST_E2E_PROFILE_04_SERVICE}
    )

    add_test(
        NAME ${TEST_E2E_PROFILE_04_NAME}_external
        COMMAND ${NETWORK_TEST_BIN_DIR}/e2e_tests/${TEST_E2E_PROFILE_04_EXTERNAL_MASTER_START_SCRIPT} e2e_profile_04_test_client_external.json)
    set_tests_properties(${TEST_E2E_PROFILE_04_NAME}_external PROPERTIES TIMEOUT 180)

    add_dependencies(${TEST_E2E_PROFILE_04_SERVICE} gtest)
    add_dependencies(${TEST_E2E_PROFILE_04_CLIENT} gtest)

    add_dependencies(build_network_tests ${TEST_E2E_PROFILE_04_SERVICE})
    add_dependencies(build_network_tests ${TEST_E2E_PROFILE_04_CLIENT})

endif()

##############################################################################
# e2e profile 07 test
##############################################################################

if(${TEST_E2E_PROFILE_07})
    set(TEST_E2E_PROFILE_07_NAME e2e_profile_07_test)
    set(TEST_E2E_PROFILE_07_SERVICE e2e_profile_07_test_service)
    set(TEST_E2E_PROFILE_07_CLIENT e2e_profile_07_test_client)

    add_executable(${TEST_E2E_PROFILE_07_SERVICE} ${TEST_E2E_PROFILE_07_SERVICE}.cpp)
    target_link_libraries(${TEST_E2E_PROFILE_07_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    add_executable(${TEST_E2E_PROFILE_07_CLIENT} ${TEST_E2E_PROFILE_07_CLIENT}.cpp
    )
    target_link_libraries(${TEST_E2E_PROFILE_07_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy service config file for external allow tests into $BUILDDIR/test
    set(TEST_E2E_PROFILE_07_SERVICE_CONFIG_FILE_EXTERNAL ${TEST_E2E_PROFILE_07_NAME}_service_external.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/conf/${TEST_E2E_PROFILE_07_SERVICE_CONFIG_FILE_EXTERNAL}.in
        ${NETWORK_TEST_BIN_DIR}/e2e_tests/${TEST_E2E_PROFILE_07_SERVICE_CONFIG_FILE_EXTERNAL}
        @ONLY)

    # Copy client config file for external allow tests into $BUILDDIR/test
    set(TEST_E2E_PROFILE_07_CLIENT_CONFIG_FILE_EXTERNAL ${TEST_E2E_PROFILE_07_NAME}_client_external.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/conf/${TEST_E2E_PROFILE_07_CLIENT_CONFIG_FILE_EXTERNAL}.in
        ${NETWORK_TEST_BIN_DIR}/e2e_tests/${TEST_E2E_PROFILE_07_CLIENT_CONFIG_FILE_EXTERNAL}
        @ONLY)

    # Copy bashscript to start external tests (master) into $BUILDDIR/test
    set(TEST_E2E_PROFILE_07_EXTERNAL_MASTER_START_SCRIPT ${TEST_E2E_PROFILE_07_NAME}_external_master_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_PROFILE_07_EXTERNAL_MASTER_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/e2e_tests/${TEST_E2E_PROFILE_07_EXTERNAL_MASTER_START_SCRIPT}
        ${TEST_E2E_PROFILE_07_SERVICE}
    )

    # Copy bashscript to start external tests (slave) into $BUILDDIR/test
    set(TEST_E2E_PROFILE_07_EXTERNAL_SLAVE_START_SCRIPT ${TEST_E2E_PROFILE_07_NAME}_external_slave_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_PROFILE_07_EXTERNAL_SLAVE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/e2e_tests/${TEST_E2E_PROFILE_07_EXTERNAL_SLAVE_START_SCRIPT}
        ${TEST_E2E_PROFILE_07_SERVICE}
    )

    add_dependencies(${TEST_E2E_PROFILE_07_SERVICE} gtest)
    add_dependencies(${TEST_E2E_PROFILE_07_CLIENT} gtest)

    add_dependencies(build_network_tests ${TEST_E2E_PROFILE_07_SERVICE})
    add_dependencies(build_network_tests ${TEST_E2E_PROFILE_07_CLIENT})

    add_test(
        NAME ${TEST_E2E_PROFILE_07_NAME}_external
        COMMAND ${NETWORK_TEST_BIN_DIR}/e2e_tests/${TEST_E2E_PROFILE_07_EXTERNAL_MASTER_START_SCRIPT} e2e_profile_07_test_client_external.json)
    set_tests_properties(${TEST_E2E_PROFILE_07_NAME}_external PROPERTIES TIMEOUT 180)

endif()
