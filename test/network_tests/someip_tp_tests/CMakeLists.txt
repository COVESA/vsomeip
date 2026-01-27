# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project(someip_tp_tests LANGUAGES CXX)

# Configure necessary files into the build folder.
set(configuration_files
    someip_tp_test_master.json
    someip_tp_test_master_starter.sh
)
configure_files("${configuration_files}")

# Add test executable.
add_executable(someip_tp_test_service
    someip_tp_test_service.cpp
)

# Add test executable.
add_executable(someip_tp_test_msg_sender
    someip_tp_test_msg_sender.cpp
)

# Link vsomeip-sd to test executable.
target_link_libraries(someip_tp_test_msg_sender
    ${VSOMEIP_NAME}-sd-test
)

# Add build dependencies and link libraries to executables.
set(executables
    someip_tp_test_service
    someip_tp_test_msg_sender
)
targets_add_default_dependencies("${executables}")
targets_link_default_libraries("${executables}")

# Add custom test command.
add_custom_test(
    NAME someip_tp_test_in_sequence
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/someip_tp_test_master_starter.sh IN_SEQUENCE
)

# Add custom test command.
add_custom_test(
    NAME someip_tp_test_mixed
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/someip_tp_test_master_starter.sh MIXED
)

# Add custom test command.
add_custom_test(
    NAME someip_tp_test_incomplete
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/someip_tp_test_master_starter.sh INCOMPLETE
)

# Add custom test command.
add_custom_test(
    NAME someip_tp_test_duplicate
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/someip_tp_test_master_starter.sh DUPLICATE
)

# Add custom test command.
add_custom_test(
    NAME someip_tp_test_overlap
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/someip_tp_test_master_starter.sh OVERLAP
)

# Add custom test command.
add_custom_test(
    NAME someip_tp_test_overlap_front_back
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/someip_tp_test_master_starter.sh OVERLAP_FRONT_BACK
)
