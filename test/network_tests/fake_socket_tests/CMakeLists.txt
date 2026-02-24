# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project(test_with_fake_sockets LANGUAGES CXX)

# Configure necessary files into the build directory.
set(configuration_files
    multiple_client_one_process.json
    multiple_client_one_process_uds.json
    ecu_one.json
    ecu_two.json
)
configure_files("${configuration_files}")

add_executable(${PROJECT_NAME}
    helpers/app.cpp
    helpers/app_connection.cpp
    helpers/base_fake_socket_fixture.cpp
    helpers/command_message.cpp
    helpers/command_record.cpp
    helpers/service_state.cpp
    helpers/socket_manager.cpp
    helpers/sockets/fake_tcp_socket_handle.cpp
    helpers/sockets/fake_udp_socket_handle.cpp
    helpers/to_string.cpp
    helpers/someip_message.cpp
    main.cpp
    test_connection_restoration.cpp
    test_boardnet.cpp
    test_debounce.cpp
    test_uds_communication.cpp
)

target_link_libraries(${PROJECT_NAME}
    ${VSOMEIP_NAME}-test
    ${VSOMEIP_NAME}-sd-test
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)
targets_add_default_dependencies("${PROJECT_NAME}")

if (WITH_TEST_DISCOVERY)
    include(GoogleTest)
    gtest_add_tests(TARGET ${PROJECT_NAME} TEST_LIST tests)
else()
    add_custom_test(NAME ${PROJECT_NAME} COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME})
endif()
