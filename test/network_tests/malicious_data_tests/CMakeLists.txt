# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project(malicious_data_tests LANGUAGES CXX)

# Configure necessary files into the build folder.
set(configuration_files
    malicious_data_test_master.json
    malicious_data_test_master_starter.sh
)
configure_files("${configuration_files}")

# Add test executable.
add_executable(malicious_data_test_service
    malicious_data_test_service.cpp
)

add_executable(malicious_data_test_msg_sender
    malicious_data_test_msg_sender.cpp
)

# Link the vsomeip-sd library to the executable.
target_link_libraries(malicious_data_test_msg_sender
    vsomeip3-sd-test
)

# Add build dependencies and link libraries to executables.
set(executables
    malicious_data_test_service
    malicious_data_test_msg_sender
)
targets_add_default_dependencies("${executables}")
targets_link_default_libraries("${executables}")

# Add custom test command.
add_custom_test(
    NAME malicious_data_test_events
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/malicious_data_test_master_starter.sh MALICIOUS_EVENTS
)

# Add custom test command.
add_custom_test(
    NAME malicious_data_test_protocol_version
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/malicious_data_test_master_starter.sh PROTOCOL_VERSION
)

# Add custom test command.
add_custom_test(
    NAME malicious_data_test_message_type
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/malicious_data_test_master_starter.sh MESSAGE_TYPE
)

# Add custom test command.
add_custom_test(
    NAME malicious_data_test_return_code
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/malicious_data_test_master_starter.sh RETURN_CODE
)

# Add custom test command.
add_custom_test(
    NAME malicious_data_test_wrong_header_fields_udp
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/malicious_data_test_master_starter.sh WRONG_HEADER_FIELDS_UDP
)
