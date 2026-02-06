# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project(event_tests LANGUAGES CXX)

# Configure necessary files into the build directory.
set(configuration_files
    event_test_master.json
    event_test_master_starter.sh
    event_test_slave_starter.sh
    event_test_slave_tcp.json
    event_test_slave_udp.json
)
configure_files("${configuration_files}")

add_executable(event_test_client
    event_test_client.cpp
)

add_executable(event_test_service
    event_test_service.cpp
)

# Add build dependencies and link libraries to executables.
set(executables
    event_test_client
    event_test_service
)
targets_link_default_libraries("${executables}")
targets_add_default_dependencies("${executables}")

add_custom_test(
    NAME event_test_payload_fixed_udp
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/event_test_master_starter.sh PAYLOAD_FIXED UDP
    SEQUENTIAL
)

add_custom_test(
    NAME event_test_payload_fixed_tcp
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/event_test_master_starter.sh PAYLOAD_FIXED TCP
    SEQUENTIAL
)

add_custom_test(
    NAME event_test_payload_dynamic_udp
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/event_test_master_starter.sh PAYLOAD_DYNAMIC UDP
    SEQUENTIAL
)

add_custom_test(
    NAME event_test_payload_dynamic_tcp
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/event_test_master_starter.sh PAYLOAD_DYNAMIC TCP
    SEQUENTIAL
)
