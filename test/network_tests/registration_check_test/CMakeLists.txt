# Copyright (C) 2015-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project(registration_check_test LANGUAGES CXX)

# Configure necessary files into the build folder.
set(configuration_files
    registration_check_config.json
    registration_check_start.sh
    someip_sequence_checker.py
)
configure_files("${configuration_files}")

# Add test executable.
add_executable(registration_check_test
    registration_check_service.cpp
    registration_check_test.cpp
)

# Add build dependencies and link libraries to executables.
set(executables
    registration_check_test
)
targets_add_default_dependencies("${executables}")
targets_link_default_libraries("${executables}")

# Add custom test command.
add_custom_test(
    NAME registration_check_test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/registration_check_start.sh
    TIMEOUT 30
)
