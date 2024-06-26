# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(routing_tests LANGUAGES CXX)

if(NOT ${TESTS_BAT})
    set(TEST_LOCAL_ROUTING_NAME local_routing_test)
    set(TEST_LOCAL_ROUTING_SERVICE local_routing_test_service)
    add_executable(${TEST_LOCAL_ROUTING_SERVICE}
        ${TEST_LOCAL_ROUTING_SERVICE}.cpp)
    target_link_libraries(${TEST_LOCAL_ROUTING_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config file for service into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_SERVICE_CONFIG_FILE ${TEST_LOCAL_ROUTING_SERVICE}.json)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_LOCAL_ROUTING_SERVICE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/routing_tests/${TEST_LOCAL_ROUTING_SERVICE_CONFIG_FILE}
        ${TEST_LOCAL_ROUTING_SERVICE}
    )

    # Copy bashscript to start service into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_SERVICE_START_SCRIPT ${TEST_LOCAL_ROUTING_SERVICE}_start.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_LOCAL_ROUTING_SERVICE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/routing_tests/${TEST_LOCAL_ROUTING_SERVICE_START_SCRIPT}
        ${TEST_LOCAL_ROUTING_SERVICE}
    )

    set(TEST_LOCAL_ROUTING_CLIENT local_routing_test_client)
    add_executable(${TEST_LOCAL_ROUTING_CLIENT}
        ${TEST_LOCAL_ROUTING_CLIENT}.cpp)
    target_link_libraries(${TEST_LOCAL_ROUTING_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config file for client into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_CLIENT_CONFIG_FILE ${TEST_LOCAL_ROUTING_CLIENT}.json)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_LOCAL_ROUTING_CLIENT_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/routing_tests/${TEST_LOCAL_ROUTING_CLIENT_CONFIG_FILE}
        ${TEST_LOCAL_ROUTING_CLIENT}
    )

    # Copy bashscript to start client into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_CLIENT_START_SCRIPT ${TEST_LOCAL_ROUTING_CLIENT}_start.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_LOCAL_ROUTING_CLIENT_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/routing_tests/${TEST_LOCAL_ROUTING_CLIENT_START_SCRIPT}
        ${TEST_LOCAL_ROUTING_CLIENT}
    )

    # Copy bashscript to start client and server $BUILDDIR/test
    if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")
        set(TEST_LOCAL_ROUTING_STARTER local_routing_test_starter_qnx.sh)
    else()
        set(TEST_LOCAL_ROUTING_STARTER local_routing_test_starter.sh)
    endif()
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/routing_tests/conf/${TEST_LOCAL_ROUTING_STARTER}.in
        ${NETWORK_TEST_BIN_DIR}/routing_tests/${TEST_LOCAL_ROUTING_STARTER}
        @ONLY)

    ##############################################################################
    set(TEST_EXTERNAL_LOCAL_ROUTING_NAME external_local_routing_test)
    set(TEST_EXTERNAL_LOCAL_ROUTING_SERVICE external_local_routing_test_service)
    add_executable(${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}
        ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}.cpp)
    target_link_libraries(${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config file for service into $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_CONFIG_FILE ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/routing_tests/conf/${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_CONFIG_FILE}.in
        ${NETWORK_TEST_BIN_DIR}/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_CONFIG_FILE}
        @ONLY)

    # Copy bashscript to start service into $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_START_SCRIPT ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_START_SCRIPT}
        ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}
    )

    # Copy bashscript to start external client into $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_ROUTING_CLIENT external_local_routing_test_client_external)
    set(TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_START_SCRIPT ${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT}_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_START_SCRIPT}
        ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}
    )

    # Copy config file for client into $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_CONFIG_FILE ${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT}.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/routing_tests/conf/${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_CONFIG_FILE}.in
        ${NETWORK_TEST_BIN_DIR}/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_CONFIG_FILE}
        @ONLY)

    # Copy bashscript to start client and server $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_ROUTING_STARTER external_local_routing_test_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_STARTER}
        ${NETWORK_TEST_BIN_DIR}/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_STARTER}
        ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}
    )

    add_test(NAME ${TEST_LOCAL_ROUTING_NAME}
    COMMAND ${NETWORK_TEST_BIN_DIR}/routing_tests/${TEST_LOCAL_ROUTING_STARTER}
    )

    add_test(NAME ${TEST_EXTERNAL_LOCAL_ROUTING_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_STARTER}
    )
    set_tests_properties(${TEST_EXTERNAL_LOCAL_ROUTING_NAME} PROPERTIES TIMEOUT 120)

    add_dependencies(${TEST_LOCAL_ROUTING_SERVICE} gtest)
    add_dependencies(${TEST_LOCAL_ROUTING_CLIENT} gtest)
    add_dependencies(${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE} gtest)

    add_dependencies(build_network_tests ${TEST_LOCAL_ROUTING_SERVICE})
    add_dependencies(build_network_tests ${TEST_LOCAL_ROUTING_CLIENT})
    add_dependencies(build_network_tests ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE})

else()
    set(TEST_LOCAL_ROUTING_NAME local_routing_test)

    set(TEST_LOCAL_ROUTING_SERVICE local_routing_test_service)
    add_executable(${TEST_LOCAL_ROUTING_SERVICE}
        ${TEST_LOCAL_ROUTING_SERVICE}.cpp)
    target_link_libraries(${TEST_LOCAL_ROUTING_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config file for service into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_SERVICE_CONFIG_FILE ${TEST_LOCAL_ROUTING_SERVICE}.json)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_LOCAL_ROUTING_SERVICE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/routing_tests/${TEST_LOCAL_ROUTING_SERVICE_CONFIG_FILE}
        ${TEST_LOCAL_ROUTING_SERVICE}
    )

    # Copy bashscript to start service into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_SERVICE_START_SCRIPT ${TEST_LOCAL_ROUTING_SERVICE}_start.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_LOCAL_ROUTING_SERVICE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/routing_tests/${TEST_LOCAL_ROUTING_SERVICE_START_SCRIPT}
        ${TEST_LOCAL_ROUTING_SERVICE}
    )

    set(TEST_LOCAL_ROUTING_CLIENT local_routing_test_client)
    add_executable(${TEST_LOCAL_ROUTING_CLIENT}
        ${TEST_LOCAL_ROUTING_CLIENT}.cpp)
    target_link_libraries(${TEST_LOCAL_ROUTING_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config file for client into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_CLIENT_CONFIG_FILE ${TEST_LOCAL_ROUTING_CLIENT}.json)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_LOCAL_ROUTING_CLIENT_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/routing_tests/${TEST_LOCAL_ROUTING_CLIENT_CONFIG_FILE}
        ${TEST_LOCAL_ROUTING_CLIENT}
    )

    # Copy bashscript to start client into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_CLIENT_START_SCRIPT ${TEST_LOCAL_ROUTING_CLIENT}_start.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_LOCAL_ROUTING_CLIENT_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/routing_tests/${TEST_LOCAL_ROUTING_CLIENT_START_SCRIPT}
        ${TEST_LOCAL_ROUTING_CLIENT}
    )

    # Copy bashscript to start client and server $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_STARTER local_routing_test_starter.sh)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/routing_tests/conf/${TEST_LOCAL_ROUTING_STARTER}.bat.in
        ${NETWORK_TEST_BIN_DIR}/routing_tests/${TEST_LOCAL_ROUTING_STARTER}
        @ONLY)

    add_test(NAME ${TEST_LOCAL_ROUTING_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/routing_tests/${TEST_LOCAL_ROUTING_STARTER}
    )

    add_dependencies(${TEST_LOCAL_ROUTING_SERVICE} gtest)
    add_dependencies(${TEST_LOCAL_ROUTING_CLIENT} gtest)

    add_dependencies(build_network_tests ${TEST_LOCAL_ROUTING_SERVICE})
    add_dependencies(build_network_tests ${TEST_LOCAL_ROUTING_CLIENT})
endif()
