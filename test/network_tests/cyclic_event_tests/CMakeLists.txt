# Copyright (C) 2024 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)
# Configures the given files into the build folder.
#
# This function looks for files in the `conf` folder with the same name as the
# given files, and ending in `.in`. It will then copy these files into the build
# folder, and replace variables marked with @VARIABLE@ with their actual value.
#
# # Example
#
# set(configuration_files
#     my_test_config.json
#     my_test_starter.sh
# )
# configure_files("${configuration_files}")
function(configure_files files)
    foreach(item ${files})
        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/conf/${item}.in
            ${CMAKE_CURRENT_BINARY_DIR}/${item}
            @ONLY
        )
    endforeach()
endfunction()

# Links the given targets to the default libraries.
#
# These include libraries that are used by nearly every test, such as vsomeip,
# boost, gtest, etc.
#
# # Example
#
# set(executables
#     my_test_service
#     my_test_client
# )
# targets_link_default_libraries("${executables}")
function(targets_link_default_libraries targets)
    foreach(target ${targets})
        target_link_libraries(${target}
            ${VSOMEIP_NAME}
            ${Boost_LIBRARIES}
            ${DL_LIBRARY}
            ${TEST_LINK_LIBRARIES}
            ${DLT_LIBRARIES}
            vsomeip_utilities
        )
    endforeach()
endfunction()

# Adds build dependencies to the given targets.
#
# These ensure that the targets will be built alongside `gtest` and
# `build_network_tests` targets.
#
# # Example
#
# set(executables
#     my_test_service
#     my_test_client
# )
# targets_add_default_dependencies("${executables}")
function(targets_add_default_dependencies targets)
    foreach(target ${targets})
        add_dependencies(${target} gtest)
        add_dependencies(build_network_tests ${target})
    endforeach()
endfunction()

# Adds a custom command to be called by ctest.
#
# Options include timeouts and environment variables.
#
# Handles the necessary steps to allow calling the test with valgrind, etc.
#
# # Example
#
# add_custom_test(
#     NAME my_test_name
#     COMMAND ${CMAKE_CURRENT_BINARY_DIR}/my_test_starter.sh ARG1 ARG2...
#     TIMEOUT 60 # Optional. Defaults to 120s.
#     ENVIRONMENT VSOMEIP_CONFIGURATION=my_test_config.json # Optional.
# )
function(add_custom_test)
    set(options "")
    set(oneValueArgs NAME TIMEOUT)
    set(multiValueArgs COMMAND ENVIRONMENT)
    cmake_parse_arguments(MY_TEST
        "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN}
    )

    # Replace "test_name" in TEST_ENTRYPOINT with the actual test name.
    string(REPLACE
        "test_name" "${MY_TEST_NAME}" custom_test_entrypoint "${TEST_ENTRYPOINT}"
    )

    # Add a custom test command.
    add_test(
        NAME ${MY_TEST_NAME}
        COMMAND ${custom_test_entrypoint} ${MY_TEST_COMMAND}
    )

    if(NOT MY_TEST_TIMEOUT)
        # Set the default timeout to 120 seconds.
        set(MY_TEST_TIMEOUT 120)
    endif()

    # Set a timeout for the test.
    set_tests_properties(${MY_TEST_NAME}
        PROPERTIES TIMEOUT ${MY_TEST_TIMEOUT}
    )

    if(MY_TEST_ENVIRONMENT)
        # Set environment variables for the test.
        set_property(
            TEST ${MY_TEST_NAME}
            APPEND PROPERTY ENVIRONMENT
            "${MY_TEST_ENVIRONMENT}"
        )
    endif()
endfunction()
# Configure necessary files into the build folder.
set(configuration_files
    service/vsomeip_events.json
    service/vsomeip_gen.json
    service/vsomeip_std.json
    cyclic_event_test_client.json
    cyclic_event_test_master_starter.sh
    cyclic_event_test_slave_starter.sh
)
configure_files("${configuration_files}")

# Add test executable.
add_executable(cyclic_event_test_service
    cyclic_event_test_service.cpp
)

# Add test executable.
add_executable(cyclic_event_test_client
    cyclic_event_test_client.cpp
)

# Add build dependencies and link libraries to executables.
set(executables
    cyclic_event_test_client
    cyclic_event_test_service
)
targets_link_default_libraries("${executables}")
targets_add_default_dependencies("${executables}")

# Skip valgrind
if (DEFINED VALGRIND_TYPE AND NOT VALGRIND_TYPE STREQUAL "")
    if (TEST_SKIP_VALGRIND_TRACKING)
        set(SUBPROCESSES ,*/cyclic_event_test_*,*/routingmanagerd)
    endif()
    set(TEST_ENTRYPOINT ${TEST_ENTRYPOINT} --trace-children-skip=*/ssh${SUBPROCESSES})
endif()

# Add custom test command.
add_custom_test(
    NAME cyclic_event_test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/cyclic_event_test_master_starter.sh
    TIMEOUT 15
)
