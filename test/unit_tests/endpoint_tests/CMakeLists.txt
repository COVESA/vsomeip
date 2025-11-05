# Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project("unit_tests_endpoint_tests" LANGUAGES CXX)

set(TEST_SRCS
    ../main.cpp
    test_timer.cpp)

# We need to set the source files directly because vsomeip doesn't export the
# timer for linking.
set(
    VSIP_SRCS
    ../../../implementation/endpoints/src/timer.cpp
    ../../../implementation/endpoints/src/abstract_socket_factory.cpp
    ../../../implementation/endpoints/src/asio_socket_factory.cpp
    ../../../implementation/endpoints/src/netlink_connector.cpp
)

add_executable(
    ${PROJECT_NAME}
    ${TEST_SRCS}
    ${VSIP_SRCS}
)

target_link_libraries(
    ${PROJECT_NAME}
    gtest
    ${VSOMEIP_NAME}
)

add_dependencies(build_unit_tests ${PROJECT_NAME})

add_test(
    NAME ${PROJECT_NAME}
    COMMAND ${PROJECT_NAME}
)
