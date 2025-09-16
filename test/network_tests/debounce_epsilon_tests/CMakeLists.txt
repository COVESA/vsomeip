# Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

# Configure necessary files into the build folder.
set(configuration_files
    debounce_epsilon_test_service.sh
    debounce_epsilon_test_service.json
    debounce_epsilon_test_client.sh
    debounce_epsilon_test_client.json
)
configure_files("${configuration_files}")

# Add test executable.
add_executable(debounce_epsilon_test_service
    debounce_epsilon_test_service.cpp
)

# Add test executable.
add_executable(debounce_epsilon_test_client
    debounce_epsilon_test_client.cpp
)

# Add build dependencies and link libraries to executables.
set(executables
    debounce_epsilon_test_service
    debounce_epsilon_test_client
)
targets_link_default_libraries("${executables}")
targets_add_default_dependencies("${executables}")

# Add custom test command.
add_custom_test(
    NAME debounce_epsilon_test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/debounce_epsilon_test_client.sh
    TIMEOUT 10
)
