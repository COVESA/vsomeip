# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project(offer_tests LANGUAGES CXX)

# Configure necessary files into the build folder.
set(configuration_files
    offer_test_big_sd_msg_master.json
    offer_test_big_sd_msg_master_starter.sh
    offer_test_big_sd_msg_slave.json
    offer_test_big_sd_msg_slave_starter.sh
    offer_test_external_master.json
    offer_test_external_local_offer_master_starter.sh
    offer_test_external_remote_offer_master_starter.sh
    offer_test_external_slave.json
    offer_test_external_slave_starter.sh
    offer_test_local.json
    offer_test_local_starter.sh
    offer_test_multiple_offerings.json
    offer_test_stress_local.json
    offer_test_stress_local_starter.sh
)
configure_files("${configuration_files}")

add_executable(offer_test_service
    offer_test_service.cpp
)

add_executable(offer_test_multiple_offerings
    offer_test_multiple_offerings.cpp
)

add_executable(offer_test_client
    offer_test_client.cpp
)

add_executable(offer_test_external_local_client
    offer_test_external_local_client.cpp
)

add_executable(offer_test_external_local_service
    offer_test_external_local_service.cpp
)

add_executable(offer_test_external_remote_service
    offer_test_external_remote_service.cpp
)

add_executable(offer_test_external_sd_msg_sender
    offer_test_external_sd_msg_sender.cpp
)

# offer_test_external_sd_msg_sender.cpp directly uses some sd symbols
target_link_libraries(offer_test_external_sd_msg_sender vsomeip3-sd-test)

add_executable(offer_test_big_sd_msg_service
    offer_test_big_sd_msg_service.cpp
)

add_executable(offer_test_big_sd_msg_client
    offer_test_big_sd_msg_client.cpp
)

# Add test executable.
add_executable(stress_tcp_subscriptions
    stress_tcp_subscriptions.cpp
)

# Add build dependencies and link libraries to executables.
set(executables
    offer_test_service
    offer_test_multiple_offerings
    offer_test_client
    offer_test_external_local_client
    offer_test_external_local_service
    offer_test_external_remote_service
    offer_test_external_sd_msg_sender
    offer_test_big_sd_msg_service
    offer_test_big_sd_msg_client
    stress_tcp_subscriptions
)
targets_add_default_dependencies("${executables}")
targets_link_default_libraries("${executables}")

add_custom_test(
    NAME offer_test_local
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/offer_test_local_starter.sh
)

add_custom_test(
    NAME offer_test_external_local_offer
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/offer_test_external_local_offer_master_starter.sh
    SEQUENTIAL
)

add_custom_test(
    NAME offer_test_external_remote_offer
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/offer_test_external_remote_offer_master_starter.sh
    SEQUENTIAL
)

add_custom_test(
    NAME offer_test_big_sd_msg
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/offer_test_big_sd_msg_master_starter.sh
    SEQUENTIAL
)

add_custom_test(
    NAME offer_test_multiple_offerings
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/offer_test_multiple_offerings
)

add_custom_test(
    NAME stress_tcp_subscriptions
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/offer_test_stress_local_starter.sh
    SEQUENTIAL
)