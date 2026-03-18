# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Configure necessary files into the build folder.
set(configuration_files
    connection_timeout_sub_ack_test_master_starter.sh
    connection_timeout_sub_ack_test_client.json
    connection_timeout_sub_ack_test_service.json
)
configure_files("${configuration_files}")

# Add client test executable.
add_executable(connection_timeout_sub_ack_test_client
    connection_timeout_sub_ack_test_client.cpp
)

# Add server test executable.
add_executable(connection_timeout_sub_ack_test_service
    connection_timeout_sub_ack_test_service.cpp
)

# Add build dependencies and link libraries to executables.
set(executables
    connection_timeout_sub_ack_test_client
    connection_timeout_sub_ack_test_service
)
targets_link_default_libraries("${executables}")
targets_add_default_dependencies("${executables}")

# Add custom test command.
add_custom_test(
    NAME connection_timeout_sub_ack_test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/connection_timeout_sub_ack_test_master_starter.sh
    SEQUENTIAL
)
