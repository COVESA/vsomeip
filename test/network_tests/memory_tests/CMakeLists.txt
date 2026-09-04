# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project(memory_tests LANGUAGES CXX)

# Configure necessary files into the build directory.
set(configuration_files
    memory_test_client.json
    memory_test_service.json
    memory_test_starter.sh
)
configure_files("${configuration_files}")

add_executable(memory_test_request_and_response_client
    memory_test_request_and_response_client.cpp
)

add_executable(memory_test_request_and_response_service
    memory_test_request_and_response_service.cpp
)

add_executable(memory_test_pending_trains_client
    memory_test_pending_trains_client.cpp
)

add_executable(memory_test_pending_trains_service
    memory_test_pending_trains_service.cpp
)

add_executable(memory_test_someip_tp_client
    memory_test_someip_tp_client.cpp
)

add_executable(memory_test_someip_tp_service
    memory_test_someip_tp_service.cpp
)

# Add build dependencies and link libraries to executables.
set(executables
    memory_test_request_and_response_client
    memory_test_request_and_response_service
    memory_test_pending_trains_client
    memory_test_pending_trains_service
    memory_test_someip_tp_client
    memory_test_someip_tp_service
)
targets_link_default_libraries("${executables}")
targets_add_default_dependencies("${executables}")

add_custom_test(
    NAME memory_test_request_and_response
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/memory_test_starter.sh memory_test_request_and_response
    SKIP_VALGRIND
    SKIP_SANITIZERS
    TIMEOUT 75
    SLAVES 1
)

add_custom_test(
    NAME memory_test_pending_trains
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/memory_test_starter.sh memory_test_pending_trains
    SKIP_VALGRIND
    SKIP_SANITIZERS
    TIMEOUT 45
    SLAVES 1
)

add_custom_test(
    NAME memory_test_someip_tp
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/memory_test_starter.sh memory_test_someip_tp
    SKIP_VALGRIND
    SKIP_SANITIZERS
    TIMEOUT 45
    SLAVES 1
)
