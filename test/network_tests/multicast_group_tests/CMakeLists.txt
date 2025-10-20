# Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Configure necessary files into the build folder.
set(configuration_files
    multicast_group_test_service.json
    multicast_group_test_client.json
    multicast_group_test_service_starter.sh
    multicast_group_test_client_starter.sh
)
configure_files("${configuration_files}")

# Add test executable for service.
add_executable(multicast_group_test_service
    multicast_group_test_service.cpp
)

# Add test executable for first client.
add_executable(multicast_group_test_first_client
    multicast_group_test_first_client.cpp
)

# Add test executable for second client.
add_executable(multicast_group_test_second_client
    multicast_group_test_second_client.cpp
)

# Add build dependencies and link libraries to executables.
set(executables
    multicast_group_test_service
    multicast_group_test_first_client
    multicast_group_test_second_client
)
targets_link_default_libraries("${executables}")
targets_add_default_dependencies("${executables}")

# Add custom test command.
add_custom_test(
    NAME multicast_group_test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/multicast_group_test_service_starter.sh
    TIMEOUT 25
)
