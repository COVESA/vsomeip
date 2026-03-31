# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project(shutdown_tests LANGUAGES CXX)

# Configure the necessary files into the build folder.
set(configuration_files
    shutdown_test_local_starter.sh
    shutdown_test_local_tcp.json
    shutdown_test_local_uds.json
)
configure_files("${configuration_files}")

# Add test executable.
add_executable(shutdown_test_service_receive
    shutdown_test_service_receive.cpp
)

# Add test executable.
add_executable(shutdown_test_client_send
    shutdown_test_client_send.cpp
)

# Add test executable.
add_executable(shutdown_test_service_send
    shutdown_test_service_send.cpp
)

# Add test executable.
add_executable(shutdown_test_client_receive
    shutdown_test_client_receive.cpp
)

# Add build dependencies and link libraries to executables.
set(test_executables
    shutdown_test_service_receive
    shutdown_test_client_send
    shutdown_test_service_send
    shutdown_test_client_receive
)
targets_link_default_libraries("${test_executables}")
targets_add_default_dependencies("${test_executables}")

# Add custom test commands.

add_custom_test(
    NAME shutdown_test_client_send_local_tcp
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/shutdown_test_local_starter.sh TCP CLIENT
    SEQUENTIAL
)
add_custom_test(
    NAME shutdown_test_client_send_local_uds
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/shutdown_test_local_starter.sh UDS CLIENT
)
add_custom_test(
    NAME shutdown_test_service_send_local_tcp
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/shutdown_test_local_starter.sh TCP SERVICE
    SEQUENTIAL
)
add_custom_test(
    NAME shutdown_test_service_send_local_uds
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/shutdown_test_local_starter.sh UDS SERVICE
)
