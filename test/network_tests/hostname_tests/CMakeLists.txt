# Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Configure necessary files into the build folder.
set(configuration_files
    hostname_test_service.json
    hostname_test_client.json
    hostname_test_starter.sh
)
configure_files("${configuration_files}")

# Add test executable.
add_executable(hostname_test_service
    hostname_test_service.cpp
)

# Add test executable.
add_executable(hostname_test_client
    hostname_test_client.cpp
)

# Add build dependencies and link libraries to executables.
set(executables
    hostname_test_client
    hostname_test_service
)
targets_link_default_libraries("${executables}")
targets_add_default_dependencies("${executables}")

# Add custom test command.
add_custom_test(
    NAME hostname_test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/hostname_test_starter.sh
    TIMEOUT 15
)
