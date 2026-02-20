# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Configure necessary files into the build folder.
set(configuration_files
    tcp_user_timeout_test_master_starter.sh
    tcp_user_timeout_test_service.json
    tcp_user_timeout_test_client.json
)
configure_files("${configuration_files}")

# Add server test executable.
add_executable(tcp_user_timeout_test_service
    tcp_user_timeout_test_service.cpp
)

# Add client test executable.
add_executable(tcp_user_timeout_test_client
    tcp_user_timeout_test_client.cpp
)

# Add build dependencies and link libraries to executables.
set(executables
    tcp_user_timeout_test_service
    tcp_user_timeout_test_client
)
targets_link_default_libraries("${executables}")
targets_add_default_dependencies("${executables}")

# Add custom test command.
add_custom_test(
    NAME tcp_user_timeout_test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/tcp_user_timeout_test_master_starter.sh
    SEQUENTIAL
)
