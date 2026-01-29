# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project(unit_tests_routing_manager_tests LANGUAGES CXX)

file(GLOB SRCS ../main.cpp routing_manager_ut_setup.cpp routing_manager_base_tests.cpp ut_set_routing_state.cpp mocks/*.cpp)

set(THREADS_PREFER_PTHREAD_FLAG ON)

# ----------------------------------------------------------------------------
# Executable and libraries to link
# ----------------------------------------------------------------------------
add_executable(${PROJECT_NAME} ${SRCS})

target_link_libraries(
    ${PROJECT_NAME}
    vsomeip3-test
    vsomeip3-cfg-test
    Threads::Threads
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    gtest
    gmock
    vsomeip_utilities
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

add_dependencies(build_unit_tests ${PROJECT_NAME})

set(TEST_SRCS
    ../main.cpp
    ut_routing_client_state_machine.cpp
)

# We need to set the source files directly because vsomeip doesn't export the
# timer for linking.
set(
    VSIP_SRCS
    ../../../implementation/endpoints/src/timer.cpp
    ../../../implementation/endpoints/src/abstract_socket_factory.cpp
    ../../../implementation/endpoints/src/asio_socket_factory.cpp
    ../../../implementation/endpoints/src/netlink_connector.cpp
    ../../../implementation/routing/src/routing_client_state_machine.cpp
)

add_executable(
    unit_tests_routing_client_state_machine
    ${TEST_SRCS}
    ${VSIP_SRCS}
)

target_link_libraries(
    unit_tests_routing_client_state_machine
    gtest
    gmock
    ${VSOMEIP_NAME}-test
    ${Boost_LIBRARIES}
)

add_dependencies(build_unit_tests unit_tests_routing_client_state_machine)

add_test(
    NAME unit_tests_routing_client_state_machine
    COMMAND unit_tests_routing_client_state_machine
)
