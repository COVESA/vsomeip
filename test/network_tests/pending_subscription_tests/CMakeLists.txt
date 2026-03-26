# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project(pending_subscription_test LANGUAGES CXX)

# Configure necessary files into the build folder.
set(configuration_files
    pending_subscription_test_master.json
    pending_subscription_test_master_starter.sh
)
configure_files("${configuration_files}")

# Add test executable.
add_executable(pending_subscription_test_service
    pending_subscription_test_service.cpp
)

# Add test executable.
add_executable(pending_subscription_test_sd_msg_sender
    pending_subscription_test_sd_msg_sender.cpp
)

# Link vsomeip-sd to executable.
target_link_libraries(pending_subscription_test_sd_msg_sender
    vsomeip3-sd-test
)

# Add build dependencies and link libraries to executables.
set(executables
    pending_subscription_test_service
    pending_subscription_test_sd_msg_sender
)
targets_add_default_dependencies("${executables}")
targets_link_default_libraries("${executables}")

# Add custom test command.
add_custom_test(
    NAME pending_subscription_test_subscribe
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/pending_subscription_test_master_starter.sh SUBSCRIBE
)

# Add custom test command.
add_custom_test(
    NAME pending_subscription_test_alternating_subscribe_unsubscribe
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/pending_subscription_test_master_starter.sh SUBSCRIBE_UNSUBSCRIBE
)

# Add custom test command.
add_custom_test(
    NAME pending_subscription_test_unsubscribe
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/pending_subscription_test_master_starter.sh UNSUBSCRIBE
)

# Add custom test command.
add_custom_test(
    NAME pending_subscription_test_alternating_subscribe_unsubscribe_nack
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/pending_subscription_test_master_starter.sh SUBSCRIBE_UNSUBSCRIBE_NACK
)

# Add custom test command.
add_custom_test(
    NAME pending_subscription_test_alternating_subscribe_unsubscribe_same_port
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/pending_subscription_test_master_starter.sh SUBSCRIBE_UNSUBSCRIBE_SAME_PORT
)

# Add custom test command.
add_custom_test(
    NAME pending_subscription_test_subscribe_resubscribe_mixed
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/pending_subscription_test_master_starter.sh SUBSCRIBE_RESUBSCRIBE_MIXED
)

# Add custom test command.
add_custom_test(
    NAME pending_subscription_test_subscribe_stopsubscribe_subscribe
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/pending_subscription_test_master_starter.sh SUBSCRIBE_STOPSUBSCRIBE_SUBSCRIBE
)

# Add custom test command.
add_custom_test(
    NAME pending_subscription_test_send_request_to_sd_port
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/pending_subscription_test_master_starter.sh REQUEST_TO_SD
)
