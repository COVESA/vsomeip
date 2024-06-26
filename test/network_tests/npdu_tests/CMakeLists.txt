# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(npdu_tests LANGUAGES CXX)

if(NOT ${TESTS_BAT} AND NOT ${CMAKE_SYSTEM_NAME} MATCHES "QNX")
    set(TEST_NPDU_NAME npdu_test)
    set(TEST_NPDU_DAEMON npdu_test_rmd)
    set(TEST_NPDU_DAEMON_CLIENT ${TEST_NPDU_DAEMON}_client_side)
    add_executable(${TEST_NPDU_DAEMON_CLIENT}
        ${TEST_NPDU_DAEMON}.cpp)
    set_target_properties(${TEST_NPDU_DAEMON_CLIENT} PROPERTIES COMPILE_FLAGS -DRMD_CLIENT_SIDE)
    target_link_libraries(${TEST_NPDU_DAEMON_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_NPDU_DAEMON_SERVICE ${TEST_NPDU_DAEMON}_service_side)
    add_executable(${TEST_NPDU_DAEMON_SERVICE}
        ${TEST_NPDU_DAEMON}.cpp)
    set_target_properties(${TEST_NPDU_DAEMON_SERVICE} PROPERTIES COMPILE_FLAGS -DRMD_SERVICE_SIDE)
    target_link_libraries(${TEST_NPDU_DAEMON_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    ##############################################################################

    set(TEST_NPDU_SERVICE npdu_test_service)

    set(TEST_NPDU_SERVICE_ONE npdu_test_service_1)
    add_executable(${TEST_NPDU_SERVICE_ONE}
        ${TEST_NPDU_SERVICE}.cpp)
    set_target_properties(${TEST_NPDU_SERVICE_ONE} PROPERTIES COMPILE_FLAGS -DSERVICE_NUMBER=0)
    target_link_libraries(${TEST_NPDU_SERVICE_ONE}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-cfg
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_NPDU_SERVICE_TWO npdu_test_service_2)
    add_executable(${TEST_NPDU_SERVICE_TWO}
        ${TEST_NPDU_SERVICE}.cpp)
    set_target_properties(${TEST_NPDU_SERVICE_TWO} PROPERTIES COMPILE_FLAGS -DSERVICE_NUMBER=1)
    target_link_libraries(${TEST_NPDU_SERVICE_TWO}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-cfg
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_NPDU_SERVICE_THREE npdu_test_service_3)
    add_executable(${TEST_NPDU_SERVICE_THREE}
        ${TEST_NPDU_SERVICE}.cpp)
    set_target_properties(${TEST_NPDU_SERVICE_THREE} PROPERTIES COMPILE_FLAGS -DSERVICE_NUMBER=2)
    target_link_libraries(${TEST_NPDU_SERVICE_THREE}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-cfg
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_NPDU_SERVICE_FOUR npdu_test_service_4)
    add_executable(${TEST_NPDU_SERVICE_FOUR}
        ${TEST_NPDU_SERVICE}.cpp)
    set_target_properties(${TEST_NPDU_SERVICE_FOUR} PROPERTIES COMPILE_FLAGS -DSERVICE_NUMBER=3)
    target_link_libraries(${TEST_NPDU_SERVICE_FOUR}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-cfg
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config file for service w/o npdu into $BUILDDIR/test
    set(TEST_NPDU_SERVICE_CONFIG_FILE ${TEST_NPDU_SERVICE}_no_npdu.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/npdu_tests/conf/${TEST_NPDU_SERVICE_CONFIG_FILE}.in
        ${NETWORK_TEST_BIN_DIR}/npdu_tests/${TEST_NPDU_SERVICE_CONFIG_FILE}
        @ONLY)

    # Copy bashscript to start service w/o npdu into $BUILDDIR/test
    set(TEST_NPDU_SERVICE_START_SCRIPT ${TEST_NPDU_SERVICE}_no_npdu_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/npdu_tests/${TEST_NPDU_SERVICE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/npdu_tests/${TEST_NPDU_SERVICE_START_SCRIPT}
        ${TEST_NPDU_SERVICE_ONE}
    )

    # Copy config file for service with npdu into $BUILDDIR/test
    set(TEST_NPDU_SERVICE_CONFIG_FILE ${TEST_NPDU_SERVICE}_npdu.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/npdu_tests/conf/${TEST_NPDU_SERVICE_CONFIG_FILE}.in
        ${NETWORK_TEST_BIN_DIR}/npdu_tests/${TEST_NPDU_SERVICE_CONFIG_FILE}
        @ONLY)

    # Copy bashscript to start service with npdu into $BUILDDIR/test
    set(TEST_NPDU_SERVICE_START_SCRIPT ${TEST_NPDU_SERVICE}_npdu_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/npdu_tests/${TEST_NPDU_SERVICE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/npdu_tests/${TEST_NPDU_SERVICE_START_SCRIPT}
        ${TEST_NPDU_SERVICE_ONE}
    )

    ##############################################################################

    set(TEST_NPDU_CLIENT npdu_test_client)

    set(TEST_NPDU_CLIENT_ONE npdu_test_client_1)
    add_executable(${TEST_NPDU_CLIENT_ONE}
        ${TEST_NPDU_CLIENT}.cpp)
    target_link_libraries(${TEST_NPDU_CLIENT_ONE}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-cfg
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_NPDU_CLIENT_TWO npdu_test_client_2)
    add_executable(${TEST_NPDU_CLIENT_TWO}
        ${TEST_NPDU_CLIENT}.cpp)
    target_link_libraries(${TEST_NPDU_CLIENT_TWO}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-cfg
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_NPDU_CLIENT_THREE npdu_test_client_3)
    add_executable(${TEST_NPDU_CLIENT_THREE}
        ${TEST_NPDU_CLIENT}.cpp)
    target_link_libraries(${TEST_NPDU_CLIENT_THREE}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-cfg
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_NPDU_CLIENT_FOUR npdu_test_client_4)
    add_executable(${TEST_NPDU_CLIENT_FOUR}
        ${TEST_NPDU_CLIENT}.cpp)
    target_link_libraries(${TEST_NPDU_CLIENT_FOUR}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-cfg
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config files for client w/o npdu into $BUILDDIR/test
    set(TEST_NPDU_CLIENT_CONFIG_FILE ${TEST_NPDU_CLIENT}_no_npdu.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/npdu_tests/conf/${TEST_NPDU_CLIENT_CONFIG_FILE}.in
        ${NETWORK_TEST_BIN_DIR}/npdu_tests/${TEST_NPDU_CLIENT_CONFIG_FILE}
        @ONLY)

    # Copy bashscript to start client w/o npdu into $BUILDDIR/test
    set(TEST_NPDU_CLIENT_START_SCRIPT ${TEST_NPDU_CLIENT}_no_npdu_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/npdu_tests/${TEST_NPDU_CLIENT_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/npdu_tests/${TEST_NPDU_CLIENT_START_SCRIPT}
        ${TEST_NPDU_CLIENT_ONE}
    )

    # Copy config file for client with npdu into $BUILDDIR/test
    set(TEST_NPDU_CLIENT_CONFIG_FILE ${TEST_NPDU_CLIENT}_npdu.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/npdu_tests/conf/${TEST_NPDU_CLIENT_CONFIG_FILE}.in
        ${NETWORK_TEST_BIN_DIR}/npdu_tests/${TEST_NPDU_CLIENT_CONFIG_FILE}
        @ONLY)

    # Copy bashscript to start client with npdu into $BUILDDIR/test
    set(TEST_NPDU_CLIENT_START_SCRIPT ${TEST_NPDU_CLIENT}_npdu_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/npdu_tests/${TEST_NPDU_CLIENT_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/npdu_tests/${TEST_NPDU_CLIENT_START_SCRIPT}
        ${TEST_NPDU_CLIENT_ONE}
    )

    ##############################################################################

    set(TEST_NPDU_STARTER ${TEST_NPDU_NAME}_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/npdu_tests/${TEST_NPDU_STARTER}
        ${NETWORK_TEST_BIN_DIR}/npdu_tests/${TEST_NPDU_STARTER}
        ${TEST_NPDU_DAEMON_CLIENT}
    )

    # npdu tests
    add_test(NAME ${TEST_NPDU_NAME}_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/npdu_tests/${TEST_NPDU_STARTER} UDP sync)
    set_tests_properties(${TEST_NPDU_NAME}_udp PROPERTIES TIMEOUT 840)

    add_test(NAME ${TEST_NPDU_NAME}_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/npdu_tests/${TEST_NPDU_STARTER} TCP sync)
    set_tests_properties(${TEST_NPDU_NAME}_tcp PROPERTIES TIMEOUT 840)

    add_dependencies(${TEST_NPDU_SERVICE_ONE} gtest)
    add_dependencies(${TEST_NPDU_SERVICE_TWO} gtest)
    add_dependencies(${TEST_NPDU_SERVICE_THREE} gtest)
    add_dependencies(${TEST_NPDU_SERVICE_FOUR} gtest)
    add_dependencies(${TEST_NPDU_CLIENT_ONE} gtest)
    add_dependencies(${TEST_NPDU_CLIENT_TWO} gtest)
    add_dependencies(${TEST_NPDU_CLIENT_THREE} gtest)
    add_dependencies(${TEST_NPDU_CLIENT_FOUR} gtest)
    add_dependencies(${TEST_NPDU_DAEMON_CLIENT} gtest)
    add_dependencies(${TEST_NPDU_DAEMON_SERVICE} gtest)

    add_dependencies(build_network_tests ${TEST_NPDU_SERVICE_ONE})
    add_dependencies(build_network_tests ${TEST_NPDU_SERVICE_TWO})
    add_dependencies(build_network_tests ${TEST_NPDU_SERVICE_THREE})
    add_dependencies(build_network_tests ${TEST_NPDU_SERVICE_FOUR})
    add_dependencies(build_network_tests ${TEST_NPDU_CLIENT_ONE})
    add_dependencies(build_network_tests ${TEST_NPDU_CLIENT_TWO})
    add_dependencies(build_network_tests ${TEST_NPDU_CLIENT_THREE})
    add_dependencies(build_network_tests ${TEST_NPDU_CLIENT_FOUR})
    add_dependencies(build_network_tests ${TEST_NPDU_DAEMON_CLIENT})
    add_dependencies(build_network_tests ${TEST_NPDU_DAEMON_SERVICE})
endif()
