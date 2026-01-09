# Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(start_stop_start_tests LANGUAGES CXX)

# Configure necessary files into the build folder.
set(configuration_files
    start_stop_start_test.json
)
configure_files("${configuration_files}")

# Add test executable.
add_executable(start_stop_start_test_manager
    start_stop_start_test_manager.cpp
)

# Add test executable.
add_executable(start_stop_start_test_app1
    start_stop_start_test_app1.cpp
)

# Add test executable.
add_executable(start_stop_start_test_app2
    start_stop_start_test_app2.cpp
)

# Add build dependencies and link libraries to executables.
set(executables
    start_stop_start_test_app1
    start_stop_start_test_app2
    start_stop_start_test_manager
)
targets_add_default_dependencies("${executables}")
targets_link_default_libraries("${executables}")

# Add custom test command.
add_custom_test(
    NAME start_stop_start_test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/start_stop_start_test_manager
    TIMEOUT 60
)
