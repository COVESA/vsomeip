# Copyright (C) 2023-2024 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project(dispatch_app_stop_tests LANGUAGES CXX)

# Add test executable.
add_executable(dispatch_app_stop_test
    dispatch_app_stop_test.cpp
)

# Add build dependencies and link libraries to executables.
set(executables dispatch_app_stop_test)

targets_link_default_libraries("${executables}")
targets_add_default_dependencies("${executables}")

# Add custom test command.
add_custom_test(
    NAME dispatch_app_stop_test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/dispatch_app_stop_test
)
