# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project(dispatch_app_stop_tests LANGUAGES CXX)

# Configure test-specific files into the build folder.
set(configuration_files
    dispatch_app.json
)
configure_files("${configuration_files}")

add_executable(dispatch_app_stop_test
    dispatch_app_stop_test.cpp
    tools/tools.cpp
)

# Add test executable.
add_executable(dispatch_app_outlives_program
    dispatch_app_outlives_program.cpp
    tools/tools.cpp
)

# Add test executable.
add_executable(dispatch_app_outlives_global
    dispatch_app_outlives_global.cpp
    tools/tools.cpp
)

# Add test executable.
add_executable(dispatch_app_outlives_struct
    dispatch_app_outlives_struct.cpp
    tools/tools.cpp
)

# Add test executable.
add_executable(dispatch_app_multiple_program
    dispatch_app_multiple_program.cpp
    tools/tools.cpp
)

# Add test executable.
add_executable(dispatch_app_multiple_global
    dispatch_app_multiple_global.cpp
    tools/tools.cpp
)

# Add test executable.
add_executable(dispatch_app_interdependent_program
    dispatch_app_interdependent_program.cpp
    tools/tools.cpp
)

# Add build dependencies and link libraries to executables.
set(executables
    dispatch_app_stop_test
    dispatch_app_outlives_program
    dispatch_app_outlives_global
    dispatch_app_outlives_struct
    dispatch_app_multiple_program
    dispatch_app_multiple_global
    dispatch_app_interdependent_program
)

targets_link_default_libraries("${executables}")
targets_add_default_dependencies("${executables}")

add_custom_test(
    NAME dispatch_app_stop_test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/dispatch_app_stop_test
    ENVIRONMENT "VSOMEIP_CONFIGURATION=${CMAKE_CURRENT_BINARY_DIR}/dispatch_app.json"
)

add_custom_test(
    NAME dispatch_app_outlives_program
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/dispatch_app_outlives_program
    ENVIRONMENT "VSOMEIP_CONFIGURATION=${CMAKE_CURRENT_BINARY_DIR}/dispatch_app.json"
)

add_custom_test(
    NAME dispatch_app_outlives_global
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/dispatch_app_outlives_global
    ENVIRONMENT "VSOMEIP_CONFIGURATION=${CMAKE_CURRENT_BINARY_DIR}/dispatch_app.json"
)

add_custom_test(
    NAME dispatch_app_outlives_struct
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/dispatch_app_outlives_struct
    ENVIRONMENT "VSOMEIP_CONFIGURATION=${CMAKE_CURRENT_BINARY_DIR}/dispatch_app.json"
)

add_custom_test(
    NAME dispatch_app_multiple_program
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/dispatch_app_multiple_program
    ENVIRONMENT "VSOMEIP_CONFIGURATION=${CMAKE_CURRENT_BINARY_DIR}/dispatch_app.json"
)

add_custom_test(
    NAME dispatch_app_multiple_global
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/dispatch_app_multiple_global
    ENVIRONMENT "VSOMEIP_CONFIGURATION=${CMAKE_CURRENT_BINARY_DIR}/dispatch_app.json"
)

add_custom_test(
    NAME dispatch_app_interdependent_program
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/dispatch_app_interdependent_program
    ENVIRONMENT "VSOMEIP_CONFIGURATION=${CMAKE_CURRENT_BINARY_DIR}/dispatch_app.json"
)
