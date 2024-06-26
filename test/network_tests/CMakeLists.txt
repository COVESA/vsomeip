# Copyright (C) 2015-2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set (VSOMEIP_NAME vsomeip3)

# Add the gtest header files to the include files
include_directories(
    .
    ${gtest_SOURCE_DIR}/include
)


set(TEST_LINK_LIBRARIES gtest)
set(NETWORK_TEST_BIN_DIR ${PROJECT_BINARY_DIR}/test/network_tests)
set(NETWORK_TEST_SRC_DIR ${PROJECT_SOURCE_DIR}/test/network_tests)
set(HOSTNAME "$ENV{HOSTNAME}")

# Function to copy files into the build directory (or anywhere else)
# On unixoid systems this function will create symlinks instead
# SOURCE_PATH: Path to the file which should be copied
# DESTINATION_PATH: destination file
# TARGET_TO_DEPEND: The copying of the file will be added as
#                   a dependency to this target
function(copy_to_builddir SOURCE_PATH DESTINATION_PATH TARGET_TO_DEPEND)
    if(${CMAKE_SYSTEM_NAME} MATCHES "Windows" OR NOT ${TEST_SYMLINK_CONFIG_FILES})
        ADD_CUSTOM_COMMAND(
            OUTPUT "${DESTINATION_PATH}"
            COMMAND ${CMAKE_COMMAND} -E copy "${SOURCE_PATH}" "${DESTINATION_PATH}"
            DEPENDS "${SOURCE_PATH}"
            COMMENT "Copying \"${SOURCE_PATH}\" into build directory"
        )
    else()
        if(${TEST_SYMLINK_CONFIG_FILES_RELATIVE})
            ADD_CUSTOM_COMMAND(
                OUTPUT "${DESTINATION_PATH}"
                # Create a relative link
                COMMAND ln -s -r "${SOURCE_PATH}" "${DESTINATION_PATH}"
                DEPENDS "${SOURCE_PATH}"
                COMMENT "Symlinking \"${SOURCE_PATH}\" into build directory"
            )
        else()
            ADD_CUSTOM_COMMAND(
                OUTPUT "${DESTINATION_PATH}"
                # Create an absolute link
                COMMAND ${CMAKE_COMMAND} -E create_symlink "${SOURCE_PATH}" "${DESTINATION_PATH}"
                DEPENDS "${SOURCE_PATH}"
                COMMENT "Symlinking \"${SOURCE_PATH}\" into build directory"
            )
        endif()
    endif()
    # Add a random number to the end of the string to avoid problems with
    # duplicate filenames
    set(FILENAME "")
    get_filename_component(FILENAME ${SOURCE_PATH} NAME )
    string(RANDOM LENGTH 4 ALPHABET 0123456789 RANDOMNUMBER)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        ADD_CUSTOM_TARGET(copy_${FILENAME}_${RANDOMNUMBER}
            DEPENDS "${DESTINATION_PATH}"
        )
        ADD_DEPENDENCIES(${TARGET_TO_DEPEND} copy_${FILENAME}_${RANDOMNUMBER})
    else()
        ADD_CUSTOM_TARGET(symlink_${FILENAME}_${RANDOMNUMBER}
            DEPENDS "${DESTINATION_PATH}"
        )
        ADD_DEPENDENCIES(${TARGET_TO_DEPEND} symlink_${FILENAME}_${RANDOMNUMBER})
    endif()
endfunction()

### ADD TESTS SUBDIRECTORIES ###
if(NOT ${TESTS_BAT})
    if(ENABLE_LEAK_SANITIZER)
        ## remove some tests when we are executing with thread, undefined, and address sanitizers
        add_subdirectory(client_id_tests)
        add_subdirectory(initial_event_tests)
        add_subdirectory(offer_tests)
        add_subdirectory(restart_routing_tests)
        add_subdirectory(subscribe_notify_one_tests)
        add_subdirectory(subscribe_notify_tests)
        add_subdirectory(suspend_resume_tests)
    endif()

    add_subdirectory(configuration_tests)
    add_subdirectory(cpu_load_tests)
    add_subdirectory(debounce_callback_tests)
    add_subdirectory(debounce_filter_tests)
    add_subdirectory(debounce_frequency_tests)
    add_subdirectory(debounce_tests)
    add_subdirectory(e2e_tests)
    add_subdirectory(event_tests)
    add_subdirectory(lazy_load_tests)
    add_subdirectory(magic_cookies_tests)
    add_subdirectory(malicious_data_tests)
    add_subdirectory(offered_services_info_test)
    add_subdirectory(pending_subscription_tests)
    add_subdirectory(second_address_tests)
    add_subdirectory(security_tests)
    add_subdirectory(someip_tp_tests)
endif()

add_subdirectory(application_tests)
add_subdirectory(routing_tests)
add_subdirectory(big_payload_tests)
add_subdirectory(header_factory_tests)
add_subdirectory(npdu_tests)
add_subdirectory(payload_tests)

# some tests require the routingmanagerd
add_dependencies(build_network_tests routingmanagerd)
