# Copyright (C) 2024 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(regression_tests LANGUAGES CXX)

# Configure necessary files into the build folder.
set(configuration_files
    climate_test_master.json
    climate_test_master_starter.sh
    climate_test_slave.json
    climate_test_slave_starter.sh
)
configure_files("${configuration_files}")

# Add test executable.
add_executable(climate_test_service
    climate_test_service.cpp
)

# Add test executable.
add_executable(climate_test_client
    climate_test_client.cpp
)

# Add build dependencies and link libraries to executables.
set(executables
    climate_test_service
    climate_test_client
)
targets_add_default_dependencies("${executables}")
targets_link_default_libraries("${executables}")

# Skip valgrind
if (DEFINED VALGRIND_TYPE AND NOT VALGRIND_TYPE STREQUAL "")
    if (TEST_SKIP_VALGRIND_TRACKING)
        set(SUBPROCESSES ,*/climate_*)
    endif()
    set(TEST_ENTRYPOINT ${TEST_ENTRYPOINT} --trace-children-skip=*/ssh${SUBPROCESSES})
endif()

# Add custom test command.
add_custom_test(
    NAME climate_test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/climate_test_master_starter.sh
    TIMEOUT 25
)
