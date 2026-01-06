# Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project(reuse_client_id_test LANGUAGES CXX)

# Configure necessary files into the build directory.
set(configuration_files
    reuse_client_id_test.json
)
configure_files("${configuration_files}")

# Add test executable.
add_executable(reuse_client_id_test_manager
    reuse_client_id_test_manager.cpp
)

add_executable(reuse_client_id_test_client
    reuse_client_id_test_client.cpp
)

# Add build dependencies and link libraries to executables.
set(executables
    reuse_client_id_test_manager
    reuse_client_id_test_client
)
targets_link_default_libraries("${executables}")
targets_add_default_dependencies("${executables}")

# Add custom test command.
add_custom_test(
    NAME reuse_client_id_test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/reuse_client_id_test_manager
)
