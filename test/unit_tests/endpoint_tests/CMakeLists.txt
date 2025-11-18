# Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project("unit_tests_endpoint_tests" LANGUAGES CXX)

set(TEST_SRCS
    ../main.cpp
    base_endpoint_fixture.cpp
    mock_endpoint_host.cpp
    mock_routing_host.cpp
    test_timer.cpp
    test_local_endpoint.cpp
    test_local_receive_buffer.cpp
)

# We need to set the source files directly because vsomeip doesn't export the
# timer for linking.
set(
    VSIP_SRCS
    ../../../implementation/endpoints/src/timer.cpp
    ../../../implementation/endpoints/src/abstract_socket_factory.cpp
    ../../../implementation/endpoints/src/asio_socket_factory.cpp
    ../../../implementation/endpoints/src/netlink_connector.cpp
    ../../../implementation/endpoints/src/local_endpoint.cpp
    ../../../implementation/endpoints/src/local_server.cpp
    ../../../implementation/endpoints/src/local_receive_buffer.cpp
    ../../../implementation/endpoints/src/local_acceptor_uds_impl.cpp
    ../../../implementation/endpoints/src/local_socket_uds_impl.cpp
    ../../../implementation/endpoints/src/local_socket.cpp
    ../../../implementation/protocol/src/command.cpp
    ../../../implementation/protocol/src/config_command.cpp
    ../../../implementation/protocol/src/assign_client_command.cpp
    ../../../implementation/protocol/src/assign_client_ack_command.cpp
    ../../../implementation/protocol/src/service_command_base.cpp
    ../../../implementation/protocol/src/offer_service_command.cpp
    ../../../implementation/configuration/src/configuration_impl.cpp
)

add_executable(
    ${PROJECT_NAME}
    ${TEST_SRCS}
    ${VSIP_SRCS}
)

target_link_libraries(
    ${PROJECT_NAME}
    gtest
    gmock
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
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

