# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project(unit_tests_routing_manager_tests LANGUAGES CXX)

set(TEST_SRCS
    ../main.cpp
    ut_routing_client_state_machine.cpp
)

add_executable(
    unit_tests_routing_client_state_machine
    ${TEST_SRCS}
)

target_link_libraries(
    unit_tests_routing_client_state_machine
    gtest
    gmock
    ${VSOMEIP_NAME}-test
    ${Boost_LIBRARIES}
    vsomeip_utilities
)

add_dependencies(build_unit_tests unit_tests_routing_client_state_machine)

add_test(
    NAME unit_tests_routing_client_state_machine
    COMMAND unit_tests_routing_client_state_machine
)
