# Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project(availability_handler_tests LANGUAGES CXX)

# Configure necessary files into the build folder.
set(configuration_files
    availability_handler_test.json
    availability_handler_test_service_starter.sh
    availability_handler_test_client_starter.sh
)
configure_files("${configuration_files}")

# Add test executable.
add_executable(availability_handler_test_manager
    availability_handler_test_manager.cpp
)

# Add test executable.
add_executable(availability_handler_test_service
    availability_handler_test_service.cpp
)

# Add test executable.
add_executable(availability_handler_test_client
    availability_handler_test_client.cpp
)

# Add build dependencies and link libraries to executables.
set(executables
    availability_handler_test_service
    availability_handler_test_client
    availability_handler_test_manager
)
targets_add_default_dependencies("${executables}")
targets_link_default_libraries("${executables}")

# Add custom test command.
add_custom_test(
    NAME availability_handler_test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/availability_handler_test_manager
    TIMEOUT 90
)
