# Copyright (C) 2023-2024 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(lazy_load_tests LANGUAGES CXX)

if (TEST_SECURITY)

    # Configure necessary files into build folder.
    set(configuration_files
        lazy_load_test_start.sh
        vsomeip/lazy_load_test_config.json
        vsomeip/vsomeip_ext/1_1/vsomeip_security.json
        vsomeip/vsomeip_policy_extensions.json
    )
    configure_files("${configuration_files}")

    # Add test executable.
    add_executable(lazy_load_test_service
        lazy_load_test_service.cpp
    )

    # Add test executable.
    add_executable(lazy_load_test_client
        lazy_load_test_client.cpp
    )

    # Add test executable.
    add_executable(lazy_load_test_lazy_client
        lazy_load_test_lazy_client.cpp
    )

    # Add build dependencies and link libraries to executables.
    set(executables
        lazy_load_test_service
        lazy_load_test_client
        lazy_load_test_lazy_client
    )
    targets_link_default_libraries("${executables}")
    targets_add_default_dependencies("${executables}")

# Skip valgrind
    if (DEFINED VALGRIND_TYPE AND NOT VALGRIND_TYPE STREQUAL "")
        set(TEST_ENTRYPOINT ${TEST_ENTRYPOINT} --trace-children-skip=*/lazy_load_test_*,*/routingmanagerd)
    endif()

    # logs won't be generated for this test as there is one application which
    # will run as uid:1 gid:1, which is not allowed to write to the log file
    list(FILTER TEST_ENTRYPOINT EXCLUDE REGEX "^--log-file=") 

    # Add custom test command.
    add_custom_test(
        NAME lazy_load_test
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/lazy_load_test_start.sh
    )

endif()
