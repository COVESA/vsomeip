# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project(application_tests LANGUAGES CXX)

# Configure necessary files and copy them to the build folder.
set(configuration_files
    application_test.json
    application_test_daemon.json
    application_test_no_dispatch_threads.json
    application_test_no_dispatch_threads_daemon.json
    application_test_starter.sh
)
configure_files("${configuration_files}")

add_executable(application_test
    application_test.cpp
)

# Add the test executable for application_test_multiple_init
add_executable(application_test_multiple_init
    application_test_multiple_init.cpp
)

# Add build dependencies and link libraries to executables.
set(executable_targets
    application_test
    application_test_multiple_init
)
targets_add_default_dependencies("${executable_targets}")
targets_link_default_libraries("${executable_targets}")

# TODO: FIXME, remove SEQUENTIAL
add_custom_test(
    NAME application_test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/application_test_starter.sh
    SEQUENTIAL
)

add_custom_test(
    NAME application_test_multiple_init
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/application_test_multiple_init
)

if (NOT ${TESTS_BAT})

    # Configure necessary files and copy them to the build folder.
    set(configuration_files
        application_test_availability_starter.sh
    )
    configure_files("${configuration_files}")

    add_executable(application_test_single_process
        application_test_single_process.cpp
    )

    if(QNX)
        target_compile_definitions(application_test_single_process
            PRIVATE _QNX_SOURCE
        )
    endif()

    add_executable(application_test_availability
        application_test_availability.cpp
    )

    # Add build dependencies and link libraries to executables.
    set(executable_targets
        application_test_single_process
        application_test_availability
    )
    targets_add_default_dependencies("${executable_targets}")
    targets_link_default_libraries("${executable_targets}")

    add_custom_test(
        NAME application_test_single_process
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/application_test_single_process
        ENVIRONMENT "VSOMEIP_CONFIGURATION=${CMAKE_CURRENT_BINARY_DIR}/application_test_single_process.json"
    )

    add_custom_test(
        NAME application_test_availability
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/application_test_availability_starter.sh
    )

endif()
