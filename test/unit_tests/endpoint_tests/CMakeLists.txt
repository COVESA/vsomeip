# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project("unit_tests_endpoint_tests" LANGUAGES CXX)

set(TEST_SRCS
    ../main.cpp
    base_endpoint_fixture.cpp
    mock_routing_host.cpp
    test_timer.cpp
    test_local_endpoint.cpp
    test_local_receive_buffer.cpp
)

# see https://github.com/google/googletest/issues/3514
# otherwise test_local_endpoint.cpp fails to compile in clang
add_compile_options(-Wno-uninitialized)

add_executable(
    ${PROJECT_NAME}
    ${TEST_SRCS}
)

target_link_libraries(
    ${PROJECT_NAME}
    gtest
    gmock
    ${VSOMEIP_NAME}-test
    ${VSOMEIP_NAME}-cfg-test
    ${Boost_LIBRARIES}
    vsomeip_utilities
)

add_dependencies(build_unit_tests ${PROJECT_NAME})

add_test(
    NAME ${PROJECT_NAME}
    COMMAND ${PROJECT_NAME}
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/conf/uds_local_endpoint_config.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/uds_local_endpoint_config.json
    @ONLY
)

