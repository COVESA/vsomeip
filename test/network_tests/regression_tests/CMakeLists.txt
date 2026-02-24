# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project(regression_tests LANGUAGES CXX)

# Configure necessary files into the build folder.
set(configuration_files
    climate_test_master.json
    climate_test_master_starter.sh
    climate_test_slave.json
    climate_test_slave_starter.sh
    invalid_event_test_master.json
    invalid_event_test_master.sh
    invalid_event_test_slave.json
    invalid_event_test_slave.sh
)
configure_files("${configuration_files}")

add_executable(climate_test_service
    climate_test_service.cpp
)

add_executable(climate_test_client
    climate_test_client.cpp
)

# Invalid event registration tests.
add_executable(invalid_event_test_service
    invalid_event_test_service.cpp
)
add_executable(invalid_event_test_client
    invalid_event_test_client.cpp
)

# Add build dependencies and link libraries to executables.
set(executables
    climate_test_service
    climate_test_client
    invalid_event_test_service
    invalid_event_test_client
)
targets_add_default_dependencies("${executables}")
targets_link_default_libraries("${executables}")

add_custom_test(
    NAME climate_test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/climate_test_master_starter.sh
    SEQUENTIAL
)

add_custom_test(
    NAME invalid_event_test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/invalid_event_test_master.sh
    SEQUENTIAL
)
