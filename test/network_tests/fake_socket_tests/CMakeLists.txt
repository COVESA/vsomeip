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
    guest.json
)
configure_files("${configuration_files}")

add_library (fake_socket_helpers OBJECT
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
)
target_link_libraries(fake_socket_helpers
    ${VSOMEIP_NAME}-test
    ${VSOMEIP_NAME}-sd-test
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

set (TEST_SUITES
    stress_routing_info_server_reconnects
    stress_routing_info_server_reoffers
    test_connection_control
    test_connection_restoration
    test_boardnet
    test_debounce
    test_uds_communication
    test_vlan_toggle_registration
)

foreach(TEST_SUITE ${TEST_SUITES})
    set(TEST_SUITE_NAME ${TEST_SUITE}_with_fake_sockets)
    add_executable(${TEST_SUITE_NAME} ${TEST_SUITE}.cpp)
    target_link_libraries(${TEST_SUITE_NAME} fake_socket_helpers)
    targets_add_default_dependencies("${TEST_SUITE_NAME}")
    if (WITH_TEST_DISCOVERY)
        include(GoogleTest)
        gtest_add_tests(TARGET ${TEST_SUITE_NAME} TEST_LIST tests)
    else()
        add_custom_test(NAME ${TEST_SUITE_NAME} COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${TEST_SUITE_NAME})
    endif()
endforeach()
