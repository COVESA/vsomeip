# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(offered_services_info_tests LANGUAGES CXX)

set(TEST_OFFERED_SERVICES_INFO_NAME offered_services_info_test)
set(TEST_OFFERED_SERVICES_INFO_SERVICE ${TEST_OFFERED_SERVICES_INFO_NAME}_service)
add_executable(${TEST_OFFERED_SERVICES_INFO_SERVICE}
    ${TEST_OFFERED_SERVICES_INFO_NAME}_service.cpp)
target_link_libraries(${TEST_OFFERED_SERVICES_INFO_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

set(TEST_OFFERED_SERVICES_INFO_CLIENT ${TEST_OFFERED_SERVICES_INFO_NAME}_client)
add_executable(${TEST_OFFERED_SERVICES_INFO_CLIENT}
    ${TEST_OFFERED_SERVICES_INFO_NAME}_client.cpp)
target_link_libraries(${TEST_OFFERED_SERVICES_INFO_CLIENT}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

# copy starter scripts into builddir
set(TEST_OFFERED_SERVICES_INFO_LOCAL_STARTER ${TEST_OFFERED_SERVICES_INFO_NAME}_local_starter.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/offered_services_info_tests/${TEST_OFFERED_SERVICES_INFO_LOCAL_STARTER}
    ${NETWORK_TEST_BIN_DIR}/offered_services_info_tests/${TEST_OFFERED_SERVICES_INFO_LOCAL_STARTER}
    ${TEST_OFFERED_SERVICES_INFO_SERVICE}
)

# Copy config file for local test into $BUILDDIR/test
set(TEST_OFFERED_SERVICES_INFO_LOCAL_CONFIG_FILE ${TEST_OFFERED_SERVICES_INFO_NAME}_local.json)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/offered_services_info_tests/${TEST_OFFERED_SERVICES_INFO_LOCAL_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/offered_services_info_tests/${TEST_OFFERED_SERVICES_INFO_LOCAL_CONFIG_FILE}
    ${TEST_OFFERED_SERVICES_INFO_SERVICE}
)

string(REPLACE "test_name" "${TEST_OFFERED_SERVICES_INFO_NAME}" TEST_OFFERED_SERVICES_INFO_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(NAME ${TEST_OFFERED_SERVICES_INFO_NAME}_local
    COMMAND ${TEST_OFFERED_SERVICES_INFO_ENTRYPOINT}
    ${NETWORK_TEST_BIN_DIR}/offered_services_info_tests/${TEST_OFFERED_SERVICES_INFO_LOCAL_STARTER})
set_tests_properties(${TEST_OFFERED_SERVICES_INFO_NAME}_local PROPERTIES TIMEOUT 180)


add_dependencies(${TEST_OFFERED_SERVICES_INFO_CLIENT} gtest)
add_dependencies(${TEST_OFFERED_SERVICES_INFO_SERVICE} gtest)

add_dependencies(build_network_tests ${TEST_OFFERED_SERVICES_INFO_CLIENT})
add_dependencies(build_network_tests ${TEST_OFFERED_SERVICES_INFO_SERVICE})
