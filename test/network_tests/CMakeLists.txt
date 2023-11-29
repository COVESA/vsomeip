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

##############################################################################
# configuration-test
##############################################################################
if(NOT ${TESTS_BAT})
    set(TEST_CONFIGURATION configuration-test)

    add_executable(${TEST_CONFIGURATION}
        configuration_tests/configuration-test.cpp
        ${PROJECT_SOURCE_DIR}/implementation/plugin/src/plugin_manager_impl.cpp
    )
    target_link_libraries(${TEST_CONFIGURATION}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-cfg
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # The following will make sure that ${TEST_CONFIGURATION_CONFIG_FILE} is copied
    # from the config folder into the test folder in the builddirectory
    # This makes it possible to call the configuration test within the build directory
    set(TEST_CONFIGURATION_CONFIG_FILE configuration-test.json)
    set(TEST_CONFIGURATION_DEPRECATED_CONFIG_FILE configuration-test-deprecated.json)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/configuration_tests/${TEST_CONFIGURATION_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CONFIGURATION_CONFIG_FILE}
        ${TEST_CONFIGURATION}
    )
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/configuration_tests/${TEST_CONFIGURATION_DEPRECATED_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CONFIGURATION_DEPRECATED_CONFIG_FILE}
        ${TEST_CONFIGURATION}
    )
endif()
##############################################################################
# application test
##############################################################################
set(TEST_APPLICATION application_test)

add_executable(${TEST_APPLICATION} application_tests/${TEST_APPLICATION}.cpp)
target_link_libraries(${TEST_APPLICATION}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)
if(NOT ${TESTS_BAT})
    set(TEST_APPLICATION_SINGLE_PROCESS_NAME ${TEST_APPLICATION}_single_process)
    add_executable(${TEST_APPLICATION_SINGLE_PROCESS_NAME} application_tests/${TEST_APPLICATION_SINGLE_PROCESS_NAME}.cpp)
    target_link_libraries(${TEST_APPLICATION_SINGLE_PROCESS_NAME}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )
    if(QNX)
        target_compile_definitions(${TEST_APPLICATION_SINGLE_PROCESS_NAME} PRIVATE _QNX_SOURCE)
    endif()

    set(TEST_APPLICATION_AVAILABILITY_NAME ${TEST_APPLICATION}_availability)
    add_executable(${TEST_APPLICATION_AVAILABILITY_NAME} application_tests/${TEST_APPLICATION_AVAILABILITY_NAME}.cpp)
    target_link_libraries(${TEST_APPLICATION_AVAILABILITY_NAME}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_APPLICATION_SINGLE_PROCESS_CONFIGURATION_FILE ${TEST_APPLICATION}_single_process.json)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/application_tests/${TEST_APPLICATION_SINGLE_PROCESS_CONFIGURATION_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_APPLICATION_SINGLE_PROCESS_CONFIGURATION_FILE}
        ${TEST_APPLICATION}_single_process
    )

    set(TEST_APPLICATION_SINGLE_PROCESS_STARTER ${TEST_APPLICATION}_single_process_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/application_tests/${TEST_APPLICATION_SINGLE_PROCESS_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_APPLICATION_SINGLE_PROCESS_STARTER}
        ${TEST_APPLICATION}_single_process
    )

    set(TEST_APPLICATION_AVAILABILITY_STARTER ${TEST_APPLICATION_AVAILABILITY_NAME}_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/application_tests/${TEST_APPLICATION_AVAILABILITY_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_APPLICATION_AVAILABILITY_STARTER}
        ${TEST_APPLICATION_SINGLE_PROCESS_NAME}
    )
endif()

set(TEST_APPLICATION_CONFIGURATION_FILE ${TEST_APPLICATION}.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/application_tests/conf/${TEST_APPLICATION_CONFIGURATION_FILE}.in
    ${NETWORK_TEST_SRC_DIR}/application_tests/${TEST_APPLICATION_CONFIGURATION_FILE}
    @ONLY)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/application_tests/${TEST_APPLICATION_CONFIGURATION_FILE}
    ${NETWORK_TEST_BIN_DIR}/${TEST_APPLICATION_CONFIGURATION_FILE}
    ${TEST_APPLICATION}
)

set(TEST_APPLICATION_CONFIGURATION_FILE_DAEMON ${TEST_APPLICATION}_daemon.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/application_tests/conf/${TEST_APPLICATION_CONFIGURATION_FILE_DAEMON}.in
    ${NETWORK_TEST_SRC_DIR}/application_tests/${TEST_APPLICATION_CONFIGURATION_FILE_DAEMON}
    @ONLY)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/application_tests/${TEST_APPLICATION_CONFIGURATION_FILE_DAEMON}
    ${NETWORK_TEST_BIN_DIR}/${TEST_APPLICATION_CONFIGURATION_FILE_DAEMON}
    ${TEST_APPLICATION}
)

set(TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE ${TEST_APPLICATION}_no_dispatch_threads.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/application_tests/conf/${TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE}.in
    ${NETWORK_TEST_SRC_DIR}/application_tests/${TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE}
    @ONLY)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/application_tests/${TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE}
    ${NETWORK_TEST_BIN_DIR}/${TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE}
    ${TEST_APPLICATION}
)

set(TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE_DAEMON ${TEST_APPLICATION}_no_dispatch_threads_daemon.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/application_tests/conf/${TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE_DAEMON}.in
    ${NETWORK_TEST_SRC_DIR}/application_tests/${TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE_DAEMON}
    @ONLY)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/application_tests/${TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE_DAEMON}
    ${NETWORK_TEST_BIN_DIR}/${TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE_DAEMON}
    ${TEST_APPLICATION}
)

set(TEST_APPLICATION_STARTER ${TEST_APPLICATION}_starter.sh)
    if(${TESTS_BAT})
        configure_file(
            ${NETWORK_TEST_SRC_DIR}/application_tests/conf/${TEST_APPLICATION_STARTER}.bat.in
            ${NETWORK_TEST_SRC_DIR}/application_tests/${TEST_APPLICATION_STARTER}
            @ONLY)
    endif()

copy_to_builddir(${NETWORK_TEST_SRC_DIR}/application_tests/${TEST_APPLICATION_STARTER}
${NETWORK_TEST_BIN_DIR}/${TEST_APPLICATION_STARTER}
${TEST_APPLICATION}
)

##############################################################################
# magic-cookies-test-client
##############################################################################
if(NOT ${TESTS_BAT})
    set(TEST_MAGIC_COOKIES_NAME magic_cookies_test)

    set(TEST_MAGIC_COOKIES_CLIENT ${TEST_MAGIC_COOKIES_NAME}_client)
    add_executable(${TEST_MAGIC_COOKIES_CLIENT} magic_cookies_tests/${TEST_MAGIC_COOKIES_CLIENT}.cpp)
    target_link_libraries(${TEST_MAGIC_COOKIES_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${CMAKE_THREAD_LIBS_INIT}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_MAGIC_COOKIES_SERVICE ${TEST_MAGIC_COOKIES_NAME}_service)
    add_executable(${TEST_MAGIC_COOKIES_SERVICE} magic_cookies_tests/${TEST_MAGIC_COOKIES_SERVICE}.cpp)
    target_link_libraries(${TEST_MAGIC_COOKIES_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${DLT_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${TEST_LINK_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config file for client into $BUILDDIR/test
    set(TEST_MAGIC_COOKIES_CLIENT_CONFIG_FILE ${TEST_MAGIC_COOKIES_CLIENT}.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/magic_cookies_tests/conf/${TEST_MAGIC_COOKIES_CLIENT_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/magic_cookies_tests/${TEST_MAGIC_COOKIES_CLIENT_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/magic_cookies_tests/${TEST_MAGIC_COOKIES_CLIENT_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_MAGIC_COOKIES_CLIENT_CONFIG_FILE}
        ${TEST_MAGIC_COOKIES_CLIENT}
    )

    # Copy bashscript to start client into $BUILDDIR/test
    set(TEST_MAGIC_COOKIES_CLIENT_START_SCRIPT ${TEST_MAGIC_COOKIES_CLIENT}_start.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/magic_cookies_tests/${TEST_MAGIC_COOKIES_CLIENT_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_MAGIC_COOKIES_CLIENT_START_SCRIPT}
        ${TEST_MAGIC_COOKIES_CLIENT}
    )

    set(TEST_MAGIC_COOKIES_SERVICE magic_cookies_test_service)
    # Copy config file for service into $BUILDDIR/test
    set(TEST_MAGIC_COOKIES_SERVICE_CONFIG_FILE ${TEST_MAGIC_COOKIES_SERVICE}.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/magic_cookies_tests/conf/${TEST_MAGIC_COOKIES_SERVICE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/magic_cookies_tests/${TEST_MAGIC_COOKIES_SERVICE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/magic_cookies_tests/${TEST_MAGIC_COOKIES_SERVICE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_MAGIC_COOKIES_SERVICE_CONFIG_FILE}
        ${TEST_MAGIC_COOKIES_SERVICE}
    )

    # Copy bashscript to start service into $BUILDDIR/test
    set(TEST_MAGIC_COOKIES_SERVICE_START_SCRIPT ${TEST_MAGIC_COOKIES_SERVICE}_start.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/magic_cookies_tests/${TEST_MAGIC_COOKIES_SERVICE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_MAGIC_COOKIES_SERVICE_START_SCRIPT}
        ${TEST_MAGIC_COOKIES_CLIENT}
    )

    # Copy bashscript to start client and server $BUILDDIR/test
    set(TEST_MAGIC_COOKIES_STARTER ${TEST_MAGIC_COOKIES_NAME}_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/magic_cookies_tests/${TEST_MAGIC_COOKIES_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_MAGIC_COOKIES_STARTER}
        ${TEST_MAGIC_COOKIES_CLIENT}
    )
endif()
##############################################################################
# someip-header-factory-test
##############################################################################
set(TEST_HEADER_FACTORY_NAME header_factory_test)

set(TEST_HEADER_FACTORY header_factory_test)
add_executable(${TEST_HEADER_FACTORY} header_factory_tests/header_factory_test.cpp)
target_link_libraries(${TEST_HEADER_FACTORY}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

##############################################################################
# Now comes the second part of the header factory test which consists of a client
# and a service both with settings file and bash scripts to start them
set(TEST_HEADER_FACTORY_CLIENT header_factory_test_client)
add_executable(${TEST_HEADER_FACTORY_CLIENT} header_factory_tests/${TEST_HEADER_FACTORY_CLIENT}.cpp)
target_link_libraries(${TEST_HEADER_FACTORY_CLIENT}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

# Copy config file for client into $BUILDDIR/test
set(TEST_HEADER_FACTORY_CLIENT_CONFIG_FILE ${TEST_HEADER_FACTORY_CLIENT}.json)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/header_factory_tests/${TEST_HEADER_FACTORY_CLIENT_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/${TEST_HEADER_FACTORY_CLIENT_CONFIG_FILE}
    ${TEST_HEADER_FACTORY_CLIENT}
)

# Copy bashscript to start client into $BUILDDIR/test
set(TEST_HEADER_FACTORY_CLIENT_START_SCRIPT ${TEST_HEADER_FACTORY_CLIENT}_start.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/header_factory_tests/${TEST_HEADER_FACTORY_CLIENT_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/${TEST_HEADER_FACTORY_CLIENT_START_SCRIPT}
    ${TEST_HEADER_FACTORY_CLIENT}
)

set(TEST_HEADER_FACTORY_SERVICE header_factory_test_service)
add_executable(${TEST_HEADER_FACTORY_SERVICE} header_factory_tests/${TEST_HEADER_FACTORY_SERVICE}.cpp)
target_link_libraries(${TEST_HEADER_FACTORY_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

# Copy config file for service into $BUILDDIR/test
set(TEST_HEADER_FACTORY_SERVICE_CONFIG_FILE ${TEST_HEADER_FACTORY_SERVICE}.json)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/header_factory_tests/${TEST_HEADER_FACTORY_SERVICE_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/${TEST_HEADER_FACTORY_SERVICE_CONFIG_FILE}
    ${TEST_HEADER_FACTORY_SERVICE}
)

# Copy bashscript to start service into $BUILDDIR/test
set(TEST_HEADER_FACTORY_SERVICE_START_SCRIPT ${TEST_HEADER_FACTORY_SERVICE}_start.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/header_factory_tests/${TEST_HEADER_FACTORY_SERVICE_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/${TEST_HEADER_FACTORY_SERVICE_START_SCRIPT}
    ${TEST_HEADER_FACTORY_SERVICE}
)

# Copy bashscript to start client and server $BUILDDIR/test
set(TEST_HEADER_FACTORY_STARTER header_factory_test_send_receive_starter.sh)

if(${TESTS_BAT})
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/header_factory_tests/conf/${TEST_HEADER_FACTORY_STARTER}.bat.in
        ${NETWORK_TEST_SRC_DIR}/header_factory_tests/${TEST_HEADER_FACTORY_STARTER}
        @ONLY)
endif()

copy_to_builddir(${NETWORK_TEST_SRC_DIR}/header_factory_tests/${TEST_HEADER_FACTORY_STARTER}
    ${NETWORK_TEST_BIN_DIR}/${TEST_HEADER_FACTORY_STARTER}
    ${TEST_HEADER_FACTORY_CLIENT}
)

##############################################################################
# routing-test
##############################################################################

if(NOT ${TESTS_BAT})
    set(TEST_LOCAL_ROUTING_NAME local_routing_test)

    set(TEST_LOCAL_ROUTING_SERVICE local_routing_test_service)
    add_executable(${TEST_LOCAL_ROUTING_SERVICE} routing_tests/${TEST_LOCAL_ROUTING_SERVICE}.cpp)
    target_link_libraries(${TEST_LOCAL_ROUTING_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config file for service into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_SERVICE_CONFIG_FILE ${TEST_LOCAL_ROUTING_SERVICE}.json)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_LOCAL_ROUTING_SERVICE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_ROUTING_SERVICE_CONFIG_FILE}
        ${TEST_LOCAL_ROUTING_SERVICE}
    )

    # Copy bashscript to start service into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_SERVICE_START_SCRIPT ${TEST_LOCAL_ROUTING_SERVICE}_start.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_LOCAL_ROUTING_SERVICE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_ROUTING_SERVICE_START_SCRIPT}
        ${TEST_LOCAL_ROUTING_SERVICE}
    )

    set(TEST_LOCAL_ROUTING_CLIENT local_routing_test_client)
    add_executable(${TEST_LOCAL_ROUTING_CLIENT} routing_tests/${TEST_LOCAL_ROUTING_CLIENT}.cpp)
    target_link_libraries(${TEST_LOCAL_ROUTING_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config file for client into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_CLIENT_CONFIG_FILE ${TEST_LOCAL_ROUTING_CLIENT}.json)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_LOCAL_ROUTING_CLIENT_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_ROUTING_CLIENT_CONFIG_FILE}
        ${TEST_LOCAL_ROUTING_CLIENT}
    )

    # Copy bashscript to start client into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_CLIENT_START_SCRIPT ${TEST_LOCAL_ROUTING_CLIENT}_start.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_LOCAL_ROUTING_CLIENT_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_ROUTING_CLIENT_START_SCRIPT}
        ${TEST_LOCAL_ROUTING_CLIENT}
    )

    # Copy bashscript to start client and server $BUILDDIR/test
    if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")
        set(TEST_LOCAL_ROUTING_STARTER local_routing_test_starter_qnx.sh)
    else()
        set(TEST_LOCAL_ROUTING_STARTER local_routing_test_starter.sh)
    endif()
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/routing_tests/conf/${TEST_LOCAL_ROUTING_STARTER}.in
        ${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_LOCAL_ROUTING_STARTER}
        @ONLY)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_LOCAL_ROUTING_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_ROUTING_STARTER}
        ${TEST_LOCAL_ROUTING_CLIENT}
    )

    ##############################################################################
    set(TEST_EXTERNAL_LOCAL_ROUTING_NAME external_local_routing_test)
    set(TEST_EXTERNAL_LOCAL_ROUTING_SERVICE external_local_routing_test_service)
    add_executable(${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE} routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}.cpp)
    target_link_libraries(${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config file for service into $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_CONFIG_FILE ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/routing_tests/conf/${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_CONFIG_FILE}
        ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}
    )

    # Copy bashscript to start service into $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_START_SCRIPT ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_START_SCRIPT}
        ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}
    )

    # Copy bashscript to start external client into $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_ROUTING_CLIENT external_local_routing_test_client_external)
    set(TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_START_SCRIPT ${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT}_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_START_SCRIPT}
        ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}
    )

    # Copy config file for client into $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_CONFIG_FILE ${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT}.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/routing_tests/conf/${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_CONFIG_FILE}
        ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}
    )

    # Copy bashscript to start client and server $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_ROUTING_STARTER external_local_routing_test_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_LOCAL_ROUTING_STARTER}
        ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}
    )
else()
    set(TEST_LOCAL_ROUTING_NAME local_routing_test)

    set(TEST_LOCAL_ROUTING_SERVICE local_routing_test_service)
    add_executable(${TEST_LOCAL_ROUTING_SERVICE} routing_tests/${TEST_LOCAL_ROUTING_SERVICE}.cpp)
    target_link_libraries(${TEST_LOCAL_ROUTING_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config file for service into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_SERVICE_CONFIG_FILE ${TEST_LOCAL_ROUTING_SERVICE}.json)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_LOCAL_ROUTING_SERVICE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_ROUTING_SERVICE_CONFIG_FILE}
        ${TEST_LOCAL_ROUTING_SERVICE}
    )

    # Copy bashscript to start service into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_SERVICE_START_SCRIPT ${TEST_LOCAL_ROUTING_SERVICE}_start.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_LOCAL_ROUTING_SERVICE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_ROUTING_SERVICE_START_SCRIPT}
        ${TEST_LOCAL_ROUTING_SERVICE}
    )

    set(TEST_LOCAL_ROUTING_CLIENT local_routing_test_client)
    add_executable(${TEST_LOCAL_ROUTING_CLIENT} routing_tests/${TEST_LOCAL_ROUTING_CLIENT}.cpp)
    target_link_libraries(${TEST_LOCAL_ROUTING_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config file for client into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_CLIENT_CONFIG_FILE ${TEST_LOCAL_ROUTING_CLIENT}.json)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_LOCAL_ROUTING_CLIENT_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_ROUTING_CLIENT_CONFIG_FILE}
        ${TEST_LOCAL_ROUTING_CLIENT}
    )

    # Copy bashscript to start client into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_CLIENT_START_SCRIPT ${TEST_LOCAL_ROUTING_CLIENT}_start.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_LOCAL_ROUTING_CLIENT_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_ROUTING_CLIENT_START_SCRIPT}
        ${TEST_LOCAL_ROUTING_CLIENT}
    )

    # Copy bashscript to start client and server $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_STARTER local_routing_test_starter.sh)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/routing_tests/conf/${TEST_LOCAL_ROUTING_STARTER}.bat.in
        ${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_LOCAL_ROUTING_STARTER}
        @ONLY)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/routing_tests/${TEST_LOCAL_ROUTING_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_ROUTING_STARTER}
        ${TEST_LOCAL_ROUTING_CLIENT}
    )
endif()
##############################################################################
# restart_routing-test
##############################################################################

if(NOT ${TESTS_BAT})
    set(TEST_RESTART_ROUTING_NAME restart_routing_test)

    set(TEST_RESTART_ROUTING_SERVICE restart_routing_test_service)
    add_executable(${TEST_RESTART_ROUTING_SERVICE} restart_routing_tests/${TEST_RESTART_ROUTING_SERVICE}.cpp)
    target_link_libraries(${TEST_RESTART_ROUTING_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config file for service into $BUILDDIR/test
    set(TEST_RESTART_ROUTING_SERVICE_CONFIG_FILE ${TEST_RESTART_ROUTING_SERVICE}.json)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/restart_routing_tests/${TEST_RESTART_ROUTING_SERVICE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_RESTART_ROUTING_SERVICE_CONFIG_FILE}
        ${TEST_RESTART_ROUTING_SERVICE}
    )

    # Copy bashscript to start service into $BUILDDIR/test
    set(TEST_RESTART_ROUTING_SERVICE_START_SCRIPT ${TEST_RESTART_ROUTING_SERVICE}_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/restart_routing_tests/${TEST_RESTART_ROUTING_SERVICE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_RESTART_ROUTING_SERVICE_START_SCRIPT}
        ${TEST_RESTART_ROUTING_SERVICE}
    )

    set(TEST_RESTART_ROUTING_CLIENT restart_routing_test_client)
    add_executable(${TEST_RESTART_ROUTING_CLIENT}
        restart_routing_tests/${TEST_RESTART_ROUTING_CLIENT}.cpp
    )
    target_link_libraries(${TEST_RESTART_ROUTING_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config file for client into $BUILDDIR/test
    set(TEST_RESTART_ROUTING_CLIENT_CONFIG_FILE ${TEST_RESTART_ROUTING_CLIENT}.json)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/restart_routing_tests/${TEST_RESTART_ROUTING_CLIENT_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_RESTART_ROUTING_CLIENT_CONFIG_FILE}
        ${TEST_RESTART_ROUTING_CLIENT}
    )

    # Copy bashscript to start client into $BUILDDIR/test
    set(TEST_RESTART_ROUTING_CLIENT_START_SCRIPT ${TEST_RESTART_ROUTING_CLIENT}_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/restart_routing_tests/${TEST_RESTART_ROUTING_CLIENT_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_RESTART_ROUTING_CLIENT_START_SCRIPT}
        ${TEST_RESTART_ROUTING_CLIENT}
    )

    # Copy bashscript to start client and server $BUILDDIR/test
    set(TEST_RESTART_ROUTING_STARTER restart_routing_test_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/restart_routing_tests/${TEST_RESTART_ROUTING_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_RESTART_ROUTING_STARTER}
        ${TEST_RESTART_ROUTING_CLIENT}
    )

    # Copy config file for autoconfig into $BUILDDIR/test
    set(TEST_RESTART_ROUTING_AUTO_CONFIG_FILE ${TEST_RESTART_ROUTING_NAME}_autoconfig.json)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/restart_routing_tests/${TEST_RESTART_ROUTING_AUTO_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_RESTART_ROUTING_AUTO_CONFIG_FILE}
        ${TEST_RESTART_ROUTING_CLIENT}
    )
endif()

##############################################################################
# security test
##############################################################################

if (${TEST_SECURITY})
    if(NOT ${TESTS_BAT})
        set(TEST_SECURITY_NAME security_test)
        set(TEST_SECURITY_SERVICE security_test_service)
        set(TEST_SECURITY_CLIENT security_test_client)

        add_executable(${TEST_SECURITY_SERVICE} security_tests/${TEST_SECURITY_SERVICE}.cpp)
        target_link_libraries(${TEST_SECURITY_SERVICE}
            ${VSOMEIP_NAME}
            ${Boost_LIBRARIES}
            ${DL_LIBRARY}
            ${TEST_LINK_LIBRARIES}
            ${DLT_LIBRARIES}
            vsomeip_utilities
        )

        # Copy config file for service into $BUILDDIR/test
        set(TEST_SECURITY_LOCAL_CONFIG_FILE ${TEST_SECURITY_NAME}_local_config.json)
        configure_file(
            ${NETWORK_TEST_SRC_DIR}/security_tests/conf/${TEST_SECURITY_LOCAL_CONFIG_FILE}.in
            ${NETWORK_TEST_SRC_DIR}/security_tests/${TEST_SECURITY_LOCAL_CONFIG_FILE}
            @ONLY)
        copy_to_builddir(
            ${NETWORK_TEST_SRC_DIR}/security_tests/${TEST_SECURITY_LOCAL_CONFIG_FILE}
            ${NETWORK_TEST_BIN_DIR}/${TEST_SECURITY_LOCAL_CONFIG_FILE}
            ${TEST_SECURITY_SERVICE}
        )

        # Copy service config file for external allow tests into $BUILDDIR/test
        set(TEST_SECURITY_SERVICE_CONFIG_FILE_EXTERNAL ${TEST_SECURITY_NAME}_config_service_external_allow.json)
        configure_file(
            ${NETWORK_TEST_SRC_DIR}/security_tests/conf/${TEST_SECURITY_SERVICE_CONFIG_FILE_EXTERNAL}.in
            ${NETWORK_TEST_SRC_DIR}/security_tests/${TEST_SECURITY_SERVICE_CONFIG_FILE_EXTERNAL}
            @ONLY)
        copy_to_builddir(
            ${NETWORK_TEST_SRC_DIR}/security_tests/${TEST_SECURITY_SERVICE_CONFIG_FILE_EXTERNAL}
            ${NETWORK_TEST_BIN_DIR}/${TEST_SECURITY_SERVICE_CONFIG_FILE_EXTERNAL}
            ${TEST_SECURITY_SERVICE}
        )

        # Copy client config file for external allow tests into $BUILDDIR/test
        set(TEST_SECURITY_CLIENT_CONFIG_FILE_EXTERNAL ${TEST_SECURITY_NAME}_config_client_external_allow.json)
        configure_file(
            ${NETWORK_TEST_SRC_DIR}/security_tests/conf/${TEST_SECURITY_CLIENT_CONFIG_FILE_EXTERNAL}.in
            ${NETWORK_TEST_SRC_DIR}/security_tests/${TEST_SECURITY_CLIENT_CONFIG_FILE_EXTERNAL}
            @ONLY)
        copy_to_builddir(
            ${NETWORK_TEST_SRC_DIR}/security_tests/${TEST_SECURITY_CLIENT_CONFIG_FILE_EXTERNAL}
            ${NETWORK_TEST_BIN_DIR}/${TEST_SECURITY_CLIENT_CONFIG_FILE_EXTERNAL}
            ${TEST_SECURITY_SERVICE}
        )

        # Copy service config file for external deny tests into $BUILDDIR/test
        set(TEST_SECURITY_SERVICE_CONFIG_FILE_EXTERNAL_DENY ${TEST_SECURITY_NAME}_config_service_external_deny.json)
        configure_file(
            ${NETWORK_TEST_SRC_DIR}/security_tests/conf/${TEST_SECURITY_SERVICE_CONFIG_FILE_EXTERNAL_DENY}.in
            ${NETWORK_TEST_SRC_DIR}/security_tests/${TEST_SECURITY_SERVICE_CONFIG_FILE_EXTERNAL_DENY}
            @ONLY)
        copy_to_builddir(
            ${NETWORK_TEST_SRC_DIR}/security_tests/${TEST_SECURITY_SERVICE_CONFIG_FILE_EXTERNAL_DENY}
            ${NETWORK_TEST_BIN_DIR}/${TEST_SECURITY_SERVICE_CONFIG_FILE_EXTERNAL_DENY}
            ${TEST_SECURITY_SERVICE}
        )

        # Copy client config file for external deny tests into $BUILDDIR/test
        set(TEST_SECURITY_CLIENT_CONFIG_FILE_EXTERNAL_DENY ${TEST_SECURITY_NAME}_config_client_external_deny.json)
        configure_file(
            ${NETWORK_TEST_SRC_DIR}/security_tests/conf/${TEST_SECURITY_CLIENT_CONFIG_FILE_EXTERNAL_DENY}.in
            ${NETWORK_TEST_SRC_DIR}/security_tests/${TEST_SECURITY_CLIENT_CONFIG_FILE_EXTERNAL_DENY}
            @ONLY)
        copy_to_builddir(
            ${NETWORK_TEST_SRC_DIR}/security_tests/${TEST_SECURITY_CLIENT_CONFIG_FILE_EXTERNAL_DENY}
            ${NETWORK_TEST_BIN_DIR}/${TEST_SECURITY_CLIENT_CONFIG_FILE_EXTERNAL_DENY}
            ${TEST_SECURITY_SERVICE}
        )

        # Copy bashscript to start local test into $BUILDDIR/test
        set(TEST_SECURITY_SERVICE_LOCAL_START_SCRIPT ${TEST_SECURITY_NAME}_local_start.sh)
        copy_to_builddir(
            ${NETWORK_TEST_SRC_DIR}/security_tests/${TEST_SECURITY_SERVICE_LOCAL_START_SCRIPT}
            ${NETWORK_TEST_BIN_DIR}/${TEST_SECURITY_SERVICE_LOCAL_START_SCRIPT}
            ${TEST_SECURITY_SERVICE}
        )

        # Copy bashscript to start external tests (master) into $BUILDDIR/test
        set(TEST_SECURITY_EXTERNAL_MASTER_START_SCRIPT ${TEST_SECURITY_NAME}_external_master_start.sh)
        copy_to_builddir(
            ${NETWORK_TEST_SRC_DIR}/security_tests/${TEST_SECURITY_EXTERNAL_MASTER_START_SCRIPT}
            ${NETWORK_TEST_BIN_DIR}/${TEST_SECURITY_EXTERNAL_MASTER_START_SCRIPT}
            ${TEST_SECURITY_SERVICE}
        )

        # Copy bashscript to start external tests (slave) into $BUILDDIR/test
        set(TEST_SECURITY_EXTERNAL_SLAVE_START_SCRIPT ${TEST_SECURITY_NAME}_external_slave_start.sh)
        copy_to_builddir(
            ${NETWORK_TEST_SRC_DIR}/security_tests/${TEST_SECURITY_EXTERNAL_SLAVE_START_SCRIPT}
            ${NETWORK_TEST_BIN_DIR}/${TEST_SECURITY_EXTERNAL_SLAVE_START_SCRIPT}
            ${TEST_SECURITY_SERVICE}
        )

        add_executable(${TEST_SECURITY_CLIENT}
            security_tests/${TEST_SECURITY_CLIENT}.cpp
        )
        target_link_libraries(${TEST_SECURITY_CLIENT}
            ${VSOMEIP_NAME}
            ${Boost_LIBRARIES}
            ${DL_LIBRARY}
            ${TEST_LINK_LIBRARIES}
            ${DLT_LIBRARIES}
            vsomeip_utilities
        )
    endif()
endif()

##############################################################################
# payload-test
##############################################################################

set(TEST_LOCAL_PAYLOAD_NAME local_payload_test)

set(TEST_PAYLOAD_SERVICE payload_test_service)
add_executable(${TEST_PAYLOAD_SERVICE} payload_tests/${TEST_PAYLOAD_SERVICE}.cpp)
target_link_libraries(${TEST_PAYLOAD_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

# Copy config file for service into $BUILDDIR/test
set(TEST_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE local_${TEST_PAYLOAD_SERVICE}.json)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE}
    ${TEST_PAYLOAD_SERVICE}
)

# Copy bashscript to start service into $BUILDDIR/test
set(TEST_LOCAL_PAYLOAD_SERVICE_START_SCRIPT local_${TEST_PAYLOAD_SERVICE}_start.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_LOCAL_PAYLOAD_SERVICE_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_PAYLOAD_SERVICE_START_SCRIPT}
    ${TEST_PAYLOAD_SERVICE}
)

set(TEST_PAYLOAD_CLIENT payload_test_client)
add_executable(${TEST_PAYLOAD_CLIENT}
    payload_tests/${TEST_PAYLOAD_CLIENT}.cpp
    payload_tests/stopwatch.cpp
)
target_link_libraries(${TEST_PAYLOAD_CLIENT}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

# Copy config file for client into $BUILDDIR/test
set(TEST_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE local_${TEST_PAYLOAD_CLIENT}.json)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE}
    ${TEST_PAYLOAD_CLIENT}
)

# Copy bashscript to start client into $BUILDDIR/test
set(TEST_LOCAL_PAYLOAD_CLIENT_START_SCRIPT local_${TEST_PAYLOAD_CLIENT}_start.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_LOCAL_PAYLOAD_CLIENT_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_PAYLOAD_CLIENT_START_SCRIPT}
    ${TEST_PAYLOAD_CLIENT}
)

# Copy bashscript to start client and server $BUILDDIR/test
set(TEST_LOCAL_PAYLOAD_STARTER local_payload_test_starter.sh)

if(${TESTS_BAT})
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/payload_tests/conf/${TEST_LOCAL_PAYLOAD_STARTER}.bat.in
        ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_LOCAL_PAYLOAD_STARTER}
        @ONLY)
endif()

copy_to_builddir(${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_LOCAL_PAYLOAD_STARTER}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_PAYLOAD_STARTER}
    ${TEST_PAYLOAD_CLIENT}
)

##############################################################################
set(TEST_EXTERNAL_LOCAL_PAYLOAD_NAME external_local_payload_test)
set(TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE external_local_payload_test_service)

# Copy config file for service into $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE ${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE}.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/conf/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE}.in
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE}
    ${TEST_PAYLOAD_SERVICE}
)

# Copy bashscript to start service into $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_START_SCRIPT ${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE}_start.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_START_SCRIPT}
    ${TEST_PAYLOAD_SERVICE}
)

set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL external_local_payload_test_client_local)

# Copy config file for client into $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL}.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/conf/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE}.in
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE}
    ${TEST_PAYLOAD_CLIENT}
)

# Copy bashscript to start client into $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_START_SCRIPT ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL}_start.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_START_SCRIPT}
    ${TEST_PAYLOAD_CLIENT}
)

# Copy bashscript to start client and server $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_NAME external_local_payload_test_client_local)
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_STARTER ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_NAME}_starter.sh)

if(${TESTS_BAT})
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/payload_tests/conf/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_STARTER}.bat.in
        ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_STARTER}
        @ONLY)
endif()

copy_to_builddir(${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_STARTER}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_STARTER}
    ${TEST_PAYLOAD_CLIENT}
)
##############################################################################
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_NAME external_local_payload_test_client_external)
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL external_local_payload_test_client_external)

# Copy config file for client into $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_CONFIG_FILE ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL}.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/conf/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_CONFIG_FILE}.in
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_CONFIG_FILE}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_CONFIG_FILE}
    ${TEST_PAYLOAD_CLIENT}
)

# Copy bashscript to start client into $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_START_SCRIPT ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL}_start.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_START_SCRIPT}
    ${TEST_PAYLOAD_CLIENT}
)

set(TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CLIENT_EXTERNAL external_local_payload_test_service_client_external)

# Copy bashscript to start service into $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CLIENTT_EXTERNAL_START_SCRIPT ${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CLIENT_EXTERNAL}_start.sh)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CLIENTT_EXTERNAL_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CLIENTT_EXTERNAL_START_SCRIPT}
    ${TEST_PAYLOAD_SERVICE}
)

# Copy bashscript to start client and server $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_STARTER ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_NAME}_starter.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_STARTER}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_STARTER}
    ${TEST_PAYLOAD_CLIENT}
)

##############################################################################
# Copy bashscript to start client and server $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_NAME external_local_payload_test_client_local_and_external)
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_STARTER ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_NAME}_starter.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_STARTER}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_STARTER}
    ${TEST_PAYLOAD_CLIENT}
)

##############################################################################
set(TEST_LOCAL_PAYLOAD_HUGE_NAME local_payload_test_huge_payload)
# Copy bashscript to start client and server $BUILDDIR/test
set(TEST_LOCAL_PAYLOAD_HUGE_STARTER ${TEST_LOCAL_PAYLOAD_HUGE_NAME}_starter.sh)

if(${TESTS_BAT})
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/payload_tests/conf/${TEST_LOCAL_PAYLOAD_HUGE_STARTER}.bat.in
        ${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_LOCAL_PAYLOAD_HUGE_STARTER}
        @ONLY)
endif()

copy_to_builddir(${NETWORK_TEST_SRC_DIR}/payload_tests/${TEST_LOCAL_PAYLOAD_HUGE_STARTER}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_PAYLOAD_HUGE_STARTER}
    ${TEST_PAYLOAD_CLIENT}
)

##############################################################################
# big_payload_test
##############################################################################

set(TEST_BIG_PAYLOAD_NAME big_payload_test)

set(TEST_BIG_PAYLOAD_SERVICE big_payload_test_service)
add_executable(${TEST_BIG_PAYLOAD_SERVICE} big_payload_tests/${TEST_BIG_PAYLOAD_SERVICE}.cpp)
target_link_libraries(${TEST_BIG_PAYLOAD_SERVICE}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

set(TEST_BIG_PAYLOAD_CLIENT big_payload_test_client)
add_executable(${TEST_BIG_PAYLOAD_CLIENT} big_payload_tests/${TEST_BIG_PAYLOAD_CLIENT}.cpp)
target_link_libraries(${TEST_BIG_PAYLOAD_CLIENT}
    ${VSOMEIP_NAME}
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${TEST_LINK_LIBRARIES}
    ${DLT_LIBRARIES}
    vsomeip_utilities
)

# Copy config file for client and service into $BUILDDIR/test
set(TEST_LOCAL_BIG_PAYLOAD_CONFIG_FILE ${TEST_BIG_PAYLOAD_NAME}_local.json)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_BIG_PAYLOAD_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_BIG_PAYLOAD_CONFIG_FILE}
    ${TEST_BIG_PAYLOAD_SERVICE}
)

# Copy config file for client and service into $BUILDDIR/test
set(TEST_QUEUE_LIMITED_LOCAL_BIG_PAYLOAD_CONFIG_FILE ${TEST_BIG_PAYLOAD_NAME}_local_queue_limited.json)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_QUEUE_LIMITED_LOCAL_BIG_PAYLOAD_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/${TEST_QUEUE_LIMITED_LOCAL_BIG_PAYLOAD_CONFIG_FILE}
    ${TEST_BIG_PAYLOAD_SERVICE}
)

# Copy config file for client and service into $BUILDDIR/test
set(TEST_LOCAL_BIG_PAYLOAD_CONFIG_FILE_RANDOM ${TEST_BIG_PAYLOAD_NAME}_local_random.json)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_BIG_PAYLOAD_CONFIG_FILE_RANDOM}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_BIG_PAYLOAD_CONFIG_FILE_RANDOM}
    ${TEST_BIG_PAYLOAD_SERVICE}
)

# Copy config file for client and service into $BUILDDIR/test
set(TEST_LOCAL_BIG_PAYLOAD_CONFIG_FILE_LIMITED ${TEST_BIG_PAYLOAD_NAME}_local_limited.json)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_BIG_PAYLOAD_CONFIG_FILE_LIMITED}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_BIG_PAYLOAD_CONFIG_FILE_LIMITED}
    ${TEST_BIG_PAYLOAD_SERVICE}
)

# Copy config file for client and service into $BUILDDIR/test
set(TEST_LOCAL_TCP_BIG_PAYLOAD_CLIENT_CONFIG_FILE ${TEST_BIG_PAYLOAD_NAME}_local_tcp_client.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_LOCAL_TCP_BIG_PAYLOAD_CLIENT_CONFIG_FILE}.in
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_TCP_BIG_PAYLOAD_CLIENT_CONFIG_FILE}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_TCP_BIG_PAYLOAD_CLIENT_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_TCP_BIG_PAYLOAD_CLIENT_CONFIG_FILE}
    ${TEST_BIG_PAYLOAD_CLIENT}
)

set(TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_CONFIG_FILE ${TEST_BIG_PAYLOAD_NAME}_local_tcp_service.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_CONFIG_FILE}.in
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_CONFIG_FILE}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_CONFIG_FILE}
    ${TEST_BIG_PAYLOAD_SERVICE}
)

# Copy config file for client and service into $BUILDDIR/test
set(TEST_LOCAL_TCP_BIG_PAYLOAD_CLIENT_CONFIG_FILE_LIMITED ${TEST_BIG_PAYLOAD_NAME}_local_tcp_client_limited.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_LOCAL_TCP_BIG_PAYLOAD_CLIENT_CONFIG_FILE_LIMITED}.in
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_TCP_BIG_PAYLOAD_CLIENT_CONFIG_FILE_LIMITED}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_TCP_BIG_PAYLOAD_CLIENT_CONFIG_FILE_LIMITED}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_TCP_BIG_PAYLOAD_CLIENT_CONFIG_FILE_LIMITED}
    ${TEST_BIG_PAYLOAD_CLIENT}
)

set(TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_CONFIG_FILE_LIMITED ${TEST_BIG_PAYLOAD_NAME}_local_tcp_service_limited.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_CONFIG_FILE_LIMITED}.in
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_CONFIG_FILE_LIMITED}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_CONFIG_FILE_LIMITED}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_CONFIG_FILE_LIMITED}
    ${TEST_BIG_PAYLOAD_SERVICE}
)

# Copy config file for client and service into $BUILDDIR/test
set(TEST_LOCAL_TCP_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED ${TEST_BIG_PAYLOAD_NAME}_local_tcp_client_queue_limited.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_LOCAL_TCP_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED}.in
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_TCP_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_TCP_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_TCP_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED}
    ${TEST_BIG_PAYLOAD_CLIENT}
)

set(TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED ${TEST_BIG_PAYLOAD_NAME}_local_tcp_service_queue_limited.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED}.in
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED}
    ${TEST_BIG_PAYLOAD_SERVICE}
)

# Copy config file for client and service into $BUILDDIR/test
set(TEST_LOCAL_TCP_BIG_PAYLOAD_CLIENT_CONFIG_FILE_RANDOM ${TEST_BIG_PAYLOAD_NAME}_local_tcp_client_random.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_LOCAL_TCP_BIG_PAYLOAD_CLIENT_CONFIG_FILE_RANDOM}.in
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_TCP_BIG_PAYLOAD_CLIENT_CONFIG_FILE_RANDOM}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_TCP_BIG_PAYLOAD_CLIENT_CONFIG_FILE_RANDOM}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_TCP_BIG_PAYLOAD_CLIENT_CONFIG_FILE_RANDOM}
    ${TEST_BIG_PAYLOAD_CLIENT}
)

set(TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_CONFIG_FILE_RANDOM ${TEST_BIG_PAYLOAD_NAME}_local_tcp_service_random.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_CONFIG_FILE_RANDOM}.in
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_CONFIG_FILE_RANDOM}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_CONFIG_FILE_RANDOM}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_CONFIG_FILE_RANDOM}
    ${TEST_BIG_PAYLOAD_SERVICE}
)

# Copy config file for client and service into $BUILDDIR/test
set(TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE ${TEST_BIG_PAYLOAD_NAME}_tcp_client.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE}.in
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE}
    ${TEST_BIG_PAYLOAD_CLIENT}
)

# Copy config file for client and service into $BUILDDIR/test
set(TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE ${TEST_BIG_PAYLOAD_NAME}_tcp_service.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE}.in
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE}
    ${TEST_BIG_PAYLOAD_SERVICE}
)

# Copy config file for client and service into $BUILDDIR/test
set(TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_LIMITED ${TEST_BIG_PAYLOAD_NAME}_tcp_client_limited_general.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_LIMITED}.in
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_LIMITED}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_LIMITED}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_LIMITED}
    ${TEST_BIG_PAYLOAD_CLIENT}
)

# Copy config file for client and service into $BUILDDIR/test
set(TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_LIMITED ${TEST_BIG_PAYLOAD_NAME}_tcp_service_limited_general.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_LIMITED}.in
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_LIMITED}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_LIMITED}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_LIMITED}
    ${TEST_BIG_PAYLOAD_SERVICE}
)

# Copy config file for client and service into $BUILDDIR/test
set(TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_RANDOM ${TEST_BIG_PAYLOAD_NAME}_tcp_client_random.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_RANDOM}.in
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_RANDOM}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_RANDOM}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_RANDOM}
    ${TEST_BIG_PAYLOAD_CLIENT}
)

# Copy config file for client and service into $BUILDDIR/test
set(TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_RANDOM ${TEST_BIG_PAYLOAD_NAME}_tcp_service_random.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_RANDOM}.in
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_RANDOM}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_RANDOM}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_RANDOM}
    ${TEST_BIG_PAYLOAD_SERVICE}
)

# Copy config file for client and service into $BUILDDIR/test
set(TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED_GENERAL ${TEST_BIG_PAYLOAD_NAME}_tcp_client_queue_limited_general.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED_GENERAL}.in
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED_GENERAL}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED_GENERAL}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED_GENERAL}
    ${TEST_BIG_PAYLOAD_CLIENT}
)

# Copy config file for client and service into $BUILDDIR/test
set(TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED_GENERAL ${TEST_BIG_PAYLOAD_NAME}_tcp_service_queue_limited_general.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED_GENERAL}.in
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED_GENERAL}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED_GENERAL}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED_GENERAL}
    ${TEST_BIG_PAYLOAD_SERVICE}
)

# Copy config file for client and service into $BUILDDIR/test
set(TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED_SPECIFIC ${TEST_BIG_PAYLOAD_NAME}_tcp_client_queue_limited_specific.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED_SPECIFIC}.in
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED_SPECIFIC}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED_SPECIFIC}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED_SPECIFIC}
    ${TEST_BIG_PAYLOAD_CLIENT}
)

# Copy config file for client and service into $BUILDDIR/test
set(TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED_SPECIFIC ${TEST_BIG_PAYLOAD_NAME}_tcp_service_queue_limited_specific.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED_SPECIFIC}.in
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED_SPECIFIC}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED_SPECIFIC}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED_SPECIFIC}
    ${TEST_BIG_PAYLOAD_SERVICE}
)
# Copy config file for UDP client and service into $BUILDDIR/test
set(TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_UDP_CONFIG_FILE ${TEST_BIG_PAYLOAD_NAME}_udp_service.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_UDP_CONFIG_FILE}.in
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_UDP_CONFIG_FILE}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_UDP_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_UDP_CONFIG_FILE}
    ${TEST_BIG_PAYLOAD_SERVICE}
)
set(TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_UDP_CONFIG_FILE ${TEST_BIG_PAYLOAD_NAME}_udp_client.json)
configure_file(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_UDP_CONFIG_FILE}.in
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_UDP_CONFIG_FILE}
    @ONLY)
copy_to_builddir(
    ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_UDP_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_UDP_CONFIG_FILE}
    ${TEST_BIG_PAYLOAD_CLIENT}
)

# Copy bashscript to start client local to $BUILDDIR/test
set(TEST_LOCAL_BIG_PAYLOAD_CLIENT_START_SCRIPT ${TEST_BIG_PAYLOAD_NAME}_client_local_start.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_BIG_PAYLOAD_CLIENT_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_BIG_PAYLOAD_CLIENT_START_SCRIPT}
    ${TEST_BIG_PAYLOAD_CLIENT}
)

# Copy bashscript to start service local to $BUILDDIR/test
set(TEST_LOCAL_BIG_PAYLOAD_SERVICE_START_SCRIPT ${TEST_BIG_PAYLOAD_NAME}_service_local_start.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_BIG_PAYLOAD_SERVICE_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_BIG_PAYLOAD_SERVICE_START_SCRIPT}
    ${TEST_BIG_PAYLOAD_SERVICE}
)

# Copy bashscript to start client and server $BUILDDIR/test
set(TEST_LOCAL_BIG_PAYLOAD_NAME big_payload_test_local)
set(TEST_LOCAL_BIG_PAYLOAD_NAME_RANDOM big_payload_test_local_random)
set(TEST_LOCAL_BIG_PAYLOAD_NAME_LIMITED big_payload_test_local_limited)
set(TEST_LOCAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED big_payload_test_local_queue_limited)
set(TEST_LOCAL_BIG_PAYLOAD_STARTER ${TEST_LOCAL_BIG_PAYLOAD_NAME}_starter.sh)

if(${TESTS_BAT})
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/big_payload_tests/conf/${TEST_LOCAL_BIG_PAYLOAD_STARTER}.bat.in
        ${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_BIG_PAYLOAD_STARTER}
        @ONLY)
endif()

copy_to_builddir(${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_BIG_PAYLOAD_STARTER}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_BIG_PAYLOAD_STARTER}
    ${TEST_BIG_PAYLOAD_SERVICE}
)

# Copy bashscript to start client and server $BUILDDIR/test
set(TEST_LOCAL_TCP_BIG_PAYLOAD_NAME big_payload_test_local_tcp)
set(TEST_LOCAL_TCP_BIG_PAYLOAD_NAME_RANDOM big_payload_test_local_tcp_random)
set(TEST_LOCAL_TCP_BIG_PAYLOAD_NAME_LIMITED big_payload_test_local_tcp_limited)
set(TEST_LOCAL_TCP_BIG_PAYLOAD_NAME_QUEUE_LIMITED big_payload_test_local_tcp_queue_limited)
# Copy starter as well as client/service start scripts for external tests
set(TEST_LOCAL_TCP_BIG_PAYLOAD_STARTER ${TEST_LOCAL_TCP_BIG_PAYLOAD_NAME}_starter.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_TCP_BIG_PAYLOAD_STARTER}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_TCP_BIG_PAYLOAD_STARTER}
    ${TEST_BIG_PAYLOAD_SERVICE}
)
set(TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_START_SCRIPT ${TEST_LOCAL_TCP_BIG_PAYLOAD_NAME}_service_start.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_TCP_BIG_PAYLOAD_SERVICE_START_SCRIPT}
    ${TEST_BIG_PAYLOAD_SERVICE}
)

# Copy bashscript to start client for external test into $BUILDDIR/test
set(TEST_EXTERNAL_BIG_PAYLOAD_NAME big_payload_test_external)
set(TEST_EXTERNAL_BIG_PAYLOAD_NAME_RANDOM big_payload_test_external_random)
set(TEST_EXTERNAL_BIG_PAYLOAD_NAME_LIMITED big_payload_test_external_limited)
set(TEST_EXTERNAL_BIG_PAYLOAD_NAME_LIMITED_GENERAL big_payload_test_external_limited_general)
set(TEST_EXTERNAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED_GENERAL big_payload_test_external_queue_limited_general)
set(TEST_EXTERNAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED_SPECIFIC big_payload_test_external_queue_limited_specific)
set(TEST_EXTERNAL_BIG_PAYLOAD_NAME_UDP big_payload_test_external_udp)

# Copy starter as well as client/service start scripts for external tests
set(TEST_EXTERNAL_BIG_PAYLOAD_STARTER ${TEST_EXTERNAL_BIG_PAYLOAD_NAME}_starter.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER}
    ${TEST_BIG_PAYLOAD_SERVICE}
)
set(TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_START_SCRIPT ${TEST_EXTERNAL_BIG_PAYLOAD_NAME}_service_start.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_START_SCRIPT}
    ${TEST_BIG_PAYLOAD_SERVICE}
)

set(TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_START_SCRIPT ${TEST_EXTERNAL_BIG_PAYLOAD_NAME}_client_start.sh)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_START_SCRIPT}
    ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_START_SCRIPT}
    ${TEST_BIG_PAYLOAD_CLIENT}
)

##############################################################################
# client id tests
##############################################################################
if(NOT ${TESTS_BAT})
    set(TEST_CLIENT_ID_NAME client_id_test)
    set(TEST_CLIENT_ID_SERVICE ${TEST_CLIENT_ID_NAME}_service)
    add_executable(${TEST_CLIENT_ID_SERVICE} client_id_tests/${TEST_CLIENT_ID_NAME}_service.cpp)
    target_link_libraries(${TEST_CLIENT_ID_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_CLIENT_ID_UTILITY ${TEST_CLIENT_ID_NAME}_utility)
    add_executable(${TEST_CLIENT_ID_UTILITY}
        client_id_tests/${TEST_CLIENT_ID_UTILITY}.cpp)
        #${PROJECT_SOURCE_DIR}/implementation/utility/src/utility.cpp)
    target_link_libraries(${TEST_CLIENT_ID_UTILITY}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-cfg
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config files for test into $BUILDDIR/test
    set(TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE
        ${TEST_CLIENT_ID_NAME}_diff_client_ids_diff_ports_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/conf/${TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${TEST_CLIENT_ID_SERVICE}
    )

    set(TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE
        ${TEST_CLIENT_ID_NAME}_diff_client_ids_diff_ports_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/conf/${TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_CLIENT_ID_SERVICE}
    )

    set(TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE
        ${TEST_CLIENT_ID_NAME}_diff_client_ids_same_ports_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/conf/${TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${TEST_CLIENT_ID_SERVICE}
    )

    set(TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE
        ${TEST_CLIENT_ID_NAME}_diff_client_ids_same_ports_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/conf/${TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_CLIENT_ID_SERVICE}
    )

    set(TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE
        ${TEST_CLIENT_ID_NAME}_diff_client_ids_partial_same_ports_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/conf/${TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}
        ${TEST_CLIENT_ID_SERVICE}
    )

    set(TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE
        ${TEST_CLIENT_ID_NAME}_diff_client_ids_partial_same_ports_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/conf/${TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_CLIENT_ID_SERVICE}
    )

    set(TEST_CLIENT_ID_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE
        ${TEST_CLIENT_ID_NAME}_same_client_ids_same_ports_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/conf/${TEST_CLIENT_ID_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${TEST_CLIENT_ID_SERVICE}
    )

    set(TEST_CLIENT_ID_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE
        ${TEST_CLIENT_ID_NAME}_same_client_ids_same_ports_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/conf/${TEST_CLIENT_ID_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_CLIENT_ID_SERVICE}
    )

    set(TEST_CLIENT_ID_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE
        ${TEST_CLIENT_ID_NAME}_same_client_ids_diff_ports_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/conf/${TEST_CLIENT_ID_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${TEST_CLIENT_ID_SERVICE}
    )

    set(TEST_CLIENT_ID_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE
        ${TEST_CLIENT_ID_NAME}_same_client_ids_diff_ports_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/conf/${TEST_CLIENT_ID_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_CLIENT_ID_SERVICE}
    )

    set(TEST_CLIENT_ID_UTILITY_CONFIG_FILE ${TEST_CLIENT_ID_NAME}_utility.json)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_UTILITY_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_UTILITY_CONFIG_FILE}
        ${TEST_CLIENT_ID_UTILITY}
    )
    set(TEST_CLIENT_ID_UTILITY_MASKED_511_CONFIG_FILE ${TEST_CLIENT_ID_NAME}_utility_masked_511.json)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_UTILITY_MASKED_511_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_UTILITY_MASKED_511_CONFIG_FILE}
        ${TEST_CLIENT_ID_UTILITY}
    )

    set(TEST_CLIENT_ID_UTILITY_MASKED_4095_CONFIG_FILE ${TEST_CLIENT_ID_NAME}_utility_masked_4095.json)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_UTILITY_MASKED_4095_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_UTILITY_MASKED_4095_CONFIG_FILE}
        ${TEST_CLIENT_ID_UTILITY}
    )

    set(TEST_CLIENT_ID_UTILITY_MASKED_127_CONFIG_FILE ${TEST_CLIENT_ID_NAME}_utility_masked_127.json)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_UTILITY_MASKED_127_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_UTILITY_MASKED_127_CONFIG_FILE}
        ${TEST_CLIENT_ID_UTILITY}
    )

    set(TEST_CLIENT_ID_UTILITY_MASKED_DISCONTINUOUS_511_CONFIG_FILE ${TEST_CLIENT_ID_NAME}_utility_discontinuous_masked_511.json)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_UTILITY_MASKED_DISCONTINUOUS_511_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_UTILITY_MASKED_DISCONTINUOUS_511_CONFIG_FILE}
        ${TEST_CLIENT_ID_UTILITY}
    )

    # copy starter scripts into builddir
    if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")
        set(TEST_CLIENT_ID_MASTER_STARTER ${TEST_CLIENT_ID_NAME}_master_starter_qnx.sh)
    else()
        set(TEST_CLIENT_ID_MASTER_STARTER ${TEST_CLIENT_ID_NAME}_master_starter.sh)
    endif()
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_MASTER_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_MASTER_STARTER}
        ${TEST_CLIENT_ID_SERVICE}
    )
    set(TEST_CLIENT_ID_SLAVE_STARTER ${TEST_CLIENT_ID_NAME}_slave_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/client_id_tests/${TEST_CLIENT_ID_SLAVE_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_SLAVE_STARTER}
        ${TEST_CLIENT_ID_SERVICE}
    )
endif()

##############################################################################
# debounce tests
##############################################################################
if(NOT ${TESTS_BAT})
    set(TEST_DEBOUNCE_NAME debounce_test)
    set(TEST_DEBOUNCE_CLIENT ${TEST_DEBOUNCE_NAME}_client)
    add_executable(${TEST_DEBOUNCE_CLIENT} debounce_tests/${TEST_DEBOUNCE_CLIENT}.cpp)
    target_link_libraries(${TEST_DEBOUNCE_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )
    set(TEST_DEBOUNCE_SERVICE ${TEST_DEBOUNCE_NAME}_service)
    add_executable(${TEST_DEBOUNCE_SERVICE} debounce_tests/${TEST_DEBOUNCE_SERVICE}.cpp)
    target_link_libraries(${TEST_DEBOUNCE_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # copy starter scripts into builddir
    set(TEST_DEBOUNCE_MASTER_STARTER ${TEST_DEBOUNCE_NAME}_master_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/debounce_tests/${TEST_DEBOUNCE_MASTER_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_DEBOUNCE_MASTER_STARTER}
        ${TEST_DEBOUNCE_SERVICE}
    )
    set(TEST_DEBOUNCE_SLAVE_STARTER ${TEST_DEBOUNCE_NAME}_slave_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/debounce_tests/${TEST_DEBOUNCE_SLAVE_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_DEBOUNCE_SLAVE_STARTER}
        ${TEST_DEBOUNCE_SERVICE}
    )

    # configure configurations and copy them into builddir
    set(TEST_DEBOUNCE_MASTER_CONFIG_FILE ${TEST_DEBOUNCE_SERVICE}.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/debounce_tests/conf/${TEST_DEBOUNCE_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/debounce_tests/${TEST_DEBOUNCE_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/debounce_tests/${TEST_DEBOUNCE_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_DEBOUNCE_MASTER_CONFIG_FILE}
        ${TEST_DEBOUNCE_SERVICE}
    )
    set(TEST_DEBOUNCE_SLAVE_CONFIG_FILE ${TEST_DEBOUNCE_CLIENT}.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/debounce_tests/conf/${TEST_DEBOUNCE_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/debounce_tests/${TEST_DEBOUNCE_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/debounce_tests/${TEST_DEBOUNCE_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_DEBOUNCE_SLAVE_CONFIG_FILE}
        ${TEST_DEBOUNCE_SERVICE}
    )
endif()

##############################################################################
# debounce filter tests
##############################################################################
if(NOT ${TESTS_BAT})
    set(TEST_DEBOUNCE_FILTER_NAME debounce_filter_test)
    set(TEST_DEBOUNCE_FILTER_CLIENT ${TEST_DEBOUNCE_FILTER_NAME}_client)
    add_executable(${TEST_DEBOUNCE_FILTER_CLIENT} debounce_filter_tests/${TEST_DEBOUNCE_FILTER_CLIENT}.cpp)
    target_link_libraries(${TEST_DEBOUNCE_FILTER_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )
    set(TEST_DEBOUNCE_FILTER_SERVICE ${TEST_DEBOUNCE_FILTER_NAME}_service)
    add_executable(${TEST_DEBOUNCE_FILTER_SERVICE} debounce_filter_tests/${TEST_DEBOUNCE_FILTER_SERVICE}.cpp)
    target_link_libraries(${TEST_DEBOUNCE_FILTER_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    # copy starter scripts into builddir
    set(TEST_DEBOUNCE_FILTER_MASTER_STARTER ${TEST_DEBOUNCE_FILTER_NAME}_master_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/debounce_filter_tests/${TEST_DEBOUNCE_FILTER_MASTER_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_DEBOUNCE_FILTER_MASTER_STARTER}
        ${TEST_DEBOUNCE_FILTER_SERVICE}
    )
    set(TEST_DEBOUNCE_FILTER_SLAVE_STARTER ${TEST_DEBOUNCE_FILTER_NAME}_slave_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/debounce_filter_tests/${TEST_DEBOUNCE_FILTER_SLAVE_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_DEBOUNCE_FILTER_SLAVE_STARTER}
        ${TEST_DEBOUNCE_FILTER_SERVICE}
    )

    # configure configurations and copy them into builddir
    set(TEST_DEBOUNCE_FILTER_MASTER_CONFIG_FILE ${TEST_DEBOUNCE_FILTER_SERVICE}.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/debounce_filter_tests/conf/${TEST_DEBOUNCE_FILTER_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/debounce_filter_tests/${TEST_DEBOUNCE_FILTER_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/debounce_filter_tests/${TEST_DEBOUNCE_FILTER_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_DEBOUNCE_FILTER_MASTER_CONFIG_FILE}
        ${TEST_DEBOUNCE_FILTER_SERVICE}
    )
    set(TEST_DEBOUNCE_FILTER_SLAVE_CONFIG_FILE ${TEST_DEBOUNCE_FILTER_CLIENT}.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/debounce_filter_tests/conf/${TEST_DEBOUNCE_FILTER_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/debounce_filter_tests/${TEST_DEBOUNCE_FILTER_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/debounce_filter_tests/${TEST_DEBOUNCE_FILTER_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_DEBOUNCE_FILTER_SLAVE_CONFIG_FILE}
        ${TEST_DEBOUNCE_FILTER_SERVICE}
    )
endif()

##############################################################################
# debounce frequency tests
##############################################################################
if(NOT ${TESTS_BAT})
    set(TEST_DEBOUNCE_FREQUENCY_NAME debounce_frequency_test)
    set(TEST_DEBOUNCE_FREQUENCY_CLIENT ${TEST_DEBOUNCE_FREQUENCY_NAME}_client)
    add_executable(${TEST_DEBOUNCE_FREQUENCY_CLIENT} debounce_frequency_tests/${TEST_DEBOUNCE_FREQUENCY_CLIENT}.cpp)
    target_link_libraries(${TEST_DEBOUNCE_FREQUENCY_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        vsomeip_utilities
    )
    set(TEST_DEBOUNCE_FREQUENCY_SERVICE ${TEST_DEBOUNCE_FREQUENCY_NAME}_service)
    add_executable(${TEST_DEBOUNCE_FREQUENCY_SERVICE} debounce_frequency_tests/${TEST_DEBOUNCE_FREQUENCY_SERVICE}.cpp)
    target_link_libraries(${TEST_DEBOUNCE_FREQUENCY_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        vsomeip_utilities
    )

    # copy starter scripts into builddir
    set(TEST_DEBOUNCE_FREQUENCY_MASTER_STARTER ${TEST_DEBOUNCE_FREQUENCY_NAME}_master_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/debounce_frequency_tests/${TEST_DEBOUNCE_FREQUENCY_MASTER_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_DEBOUNCE_FREQUENCY_MASTER_STARTER}
        ${TEST_DEBOUNCE_FREQUENCY_CLIENT}
    )
    set(TEST_DEBOUNCE_FREQUENCY_SLAVE_STARTER ${TEST_DEBOUNCE_FREQUENCY_NAME}_slave_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/debounce_frequency_tests/${TEST_DEBOUNCE_FREQUENCY_SLAVE_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_DEBOUNCE_FREQUENCY_SLAVE_STARTER}
        ${TEST_DEBOUNCE_FREQUENCY_SERVICE}
    )

    # configure configurations and copy them into builddir
    set(TEST_DEBOUNCE_FREQUENCY_MASTER_CONFIG_FILE ${TEST_DEBOUNCE_FREQUENCY_CLIENT}.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/debounce_frequency_tests/conf/${TEST_DEBOUNCE_FREQUENCY_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/debounce_frequency_tests/${TEST_DEBOUNCE_FREQUENCY_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/debounce_frequency_tests/${TEST_DEBOUNCE_FREQUENCY_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_DEBOUNCE_FREQUENCY_MASTER_CONFIG_FILE}
        ${TEST_DEBOUNCE_FREQUENCY_CLIENT}
    )
    set(TEST_DEBOUNCE_FREQUENCY_SLAVE_CONFIG_FILE ${TEST_DEBOUNCE_FREQUENCY_SERVICE}.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/debounce_frequency_tests/conf/${TEST_DEBOUNCE_FREQUENCY_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/debounce_frequency_tests/${TEST_DEBOUNCE_FREQUENCY_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/debounce_frequency_tests/${TEST_DEBOUNCE_FREQUENCY_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_DEBOUNCE_FREQUENCY_SLAVE_CONFIG_FILE}
        ${TEST_DEBOUNCE_FREQUENCY_SERVICE}
    )
endif()

##############################################################################
# debounce callback tests
##############################################################################
if(NOT ${TESTS_BAT})
    set(TEST_DEBOUNCE_CALLBACK_NAME debounce_callback_test)
    set(TEST_DEBOUNCE_CALLBACK_CLIENT ${TEST_DEBOUNCE_CALLBACK_NAME}_client)
    add_executable(${TEST_DEBOUNCE_CALLBACK_CLIENT} debounce_callback_tests/${TEST_DEBOUNCE_CALLBACK_CLIENT}.cpp)
    target_link_libraries(${TEST_DEBOUNCE_CALLBACK_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )
    set(TEST_DEBOUNCE_CALLBACK_SERVICE ${TEST_DEBOUNCE_CALLBACK_NAME}_service)
    add_executable(${TEST_DEBOUNCE_CALLBACK_SERVICE} debounce_callback_tests/${TEST_DEBOUNCE_CALLBACK_SERVICE}.cpp)
    target_link_libraries(${TEST_DEBOUNCE_CALLBACK_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    # copy starter scripts into builddir
    set(TEST_DEBOUNCE_CALLBACK_MASTER_STARTER ${TEST_DEBOUNCE_CALLBACK_NAME}_master_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/debounce_callback_tests/${TEST_DEBOUNCE_CALLBACK_MASTER_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_DEBOUNCE_CALLBACK_MASTER_STARTER}
        ${TEST_DEBOUNCE_CALLBACK_SERVICE}
    )
    set(TEST_DEBOUNCE_CALLBACK_SLAVE_STARTER ${TEST_DEBOUNCE_CALLBACK_NAME}_slave_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/debounce_callback_tests/${TEST_DEBOUNCE_CALLBACK_SLAVE_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_DEBOUNCE_CALLBACK_SLAVE_STARTER}
        ${TEST_DEBOUNCE_CALLBACK_CLIENT}
    )

    # configure configurations and copy them into builddir
    set(TEST_DEBOUNCE_CALLBACK_MASTER_CONFIG_FILE ${TEST_DEBOUNCE_CALLBACK_SERVICE}.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/debounce_callback_tests/conf/${TEST_DEBOUNCE_CALLBACK_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/debounce_callback_tests/${TEST_DEBOUNCE_CALLBACK_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/debounce_callback_tests/${TEST_DEBOUNCE_CALLBACK_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_DEBOUNCE_CALLBACK_MASTER_CONFIG_FILE}
        ${TEST_DEBOUNCE_CALLBACK_SERVICE}
    )
    set(TEST_DEBOUNCE_CALLBACK_SLAVE_CONFIG_FILE ${TEST_DEBOUNCE_CALLBACK_CLIENT}.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/debounce_callback_tests/conf/${TEST_DEBOUNCE_CALLBACK_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/debounce_callback_tests/${TEST_DEBOUNCE_CALLBACK_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/debounce_callback_tests/${TEST_DEBOUNCE_CALLBACK_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_DEBOUNCE_CALLBACK_SLAVE_CONFIG_FILE}
        ${TEST_DEBOUNCE_CALLBACK_CLIENT}
    )
endif()

##############################################################################
# subscribe notify tests
##############################################################################
if(NOT ${TESTS_BAT})
    set(TEST_SUBSCRIBE_NOTIFY_NAME subscribe_notify_test)
    set(TEST_SUBSCRIBE_NOTIFY_SERVICE ${TEST_SUBSCRIBE_NOTIFY_NAME}_service)
    add_executable(${TEST_SUBSCRIBE_NOTIFY_SERVICE} subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_NAME}_service.cpp)
    target_link_libraries(${TEST_SUBSCRIBE_NOTIFY_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config files for test into $BUILDDIR/test (local communication via UDS)
    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_master_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_slave_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_master_udp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_slave_udp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_master_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_slave_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_master_udp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_UDP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_slave_udp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_UDP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_UDP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_UDP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_UDP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_partial_same_ports_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_partial_same_ports_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_same_client_ids_same_ports_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_same_client_ids_same_ports_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_same_client_ids_diff_ports_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_same_client_ids_diff_ports_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_UDP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_same_service_id_master_udp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_UDP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_UDP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_UDP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_UDP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_UDP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_same_service_id_slave_udp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_UDP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_UDP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_UDP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_UDP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_MASTER_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_autoconfig_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_MASTER_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_SLAVE_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_autoconfig_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_SLAVE_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    # copy starter scripts into builddir
    if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")
        set(TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER ${TEST_SUBSCRIBE_NOTIFY_NAME}_master_starter_qnx.sh)
    else()
        set(TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER ${TEST_SUBSCRIBE_NOTIFY_NAME}_master_starter.sh)
    endif()
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )
    set(TEST_SUBSCRIBE_NOTIFY_SLAVE_STARTER ${TEST_SUBSCRIBE_NOTIFY_NAME}_slave_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SLAVE_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_SLAVE_STARTER}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    # subscribe_notify_test_one_event_two_eventgroups
    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME subscribe_notify_test_one_event_two_eventgroups)

    # service
    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SERVICE ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_service)
    add_executable(${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SERVICE}
        subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SERVICE}.cpp)
    target_link_libraries(${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )
    # client
    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_CLIENT ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_client)
    add_executable(${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_CLIENT}
        subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_CLIENT}.cpp)
    target_link_libraries(${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_UDP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_udp_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_UDP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_UDP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_UDP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_UDP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_CLIENT}
    )
    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_tcp_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_CLIENT}
    )

    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_STARTER
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_master_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_STARTER}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SERVICE}
    )
    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_STARTER
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_slave_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_STARTER}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SERVICE}
    )

    # Copy config files for test into $BUILDDIR/test (local communication via TCP)
    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_master_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_slave_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_TCP_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_master_tcp_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_TCP_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_TCP_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_TCP_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_TCP_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_slave_tcp_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_UDP_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_master_udp_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_UDP_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_UDP_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_UDP_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_UDP_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_slave_udp_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_master_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_slave_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_TCP_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_master_tcp_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_TCP_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_TCP_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_TCP_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_TCP_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_TCP_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_slave_tcp_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_TCP_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_TCP_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_TCP_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_TCP_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_UDP_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_master_udp_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_UDP_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_UDP_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_UDP_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_UDP_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_UDP_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_slave_udp_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_UDP_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_UDP_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_UDP_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_UDP_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_partial_same_ports_master_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_partial_same_ports_slave_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_same_client_ids_same_ports_master_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_same_client_ids_same_ports_slave_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_same_client_ids_diff_ports_master_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_same_client_ids_diff_ports_slave_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_SLAVE_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_UDP_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_same_service_id_master_udp_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_UDP_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_UDP_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_UDP_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_UDP_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_UDP_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_same_service_id_slave_udp_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_UDP_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_UDP_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_UDP_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_UDP_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_MASTER_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_autoconfig_master_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_MASTER_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_MASTER_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_MASTER_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_MASTER_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_SLAVE_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_autoconfig_slave_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_SLAVE_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_SLAVE_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_SLAVE_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_SLAVE_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    # copy starter scripts into builddir
    set(TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER ${TEST_SUBSCRIBE_NOTIFY_NAME}_master_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )
    set(TEST_SUBSCRIBE_NOTIFY_SLAVE_STARTER ${TEST_SUBSCRIBE_NOTIFY_NAME}_slave_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SLAVE_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_SLAVE_STARTER}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_master_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_UDP_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_udp_slave_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_UDP_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_UDP_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_UDP_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_UDP_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_CLIENT}
    )
    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_TCP_LOCAL_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_tcp_slave_local_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_TCP_LOCAL_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_TCP_LOCAL_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_TCP_LOCAL_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_TCP_LOCAL_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_CLIENT}
    )

    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_STARTER
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_master_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_STARTER}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SERVICE}
    )
    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_STARTER
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_slave_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_STARTER}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SERVICE}
    )

endif()

##############################################################################
# subscribe notify one tests
##############################################################################
if(NOT ${TESTS_BAT})
    set(TEST_SUBSCRIBE_NOTIFY_ONE_NAME subscribe_notify_one_test)
    set(TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_service)
    add_executable(${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE} subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_service.cpp)
    target_link_libraries(${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config files for test into $BUILDDIR/test
    set(TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_master_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_slave_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_master_udp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_slave_udp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE}
    )

    # copy starter scripts into builddir
    if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")
        set(TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_master_starter_qnx.sh)
    else()
        set(TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_master_starter.sh)
    endif()
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE}
    )
    set(TEST_SUBSCRIBE_NOTIFY_ONE_SLAVE_STARTER ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_slave_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_SLAVE_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_SLAVE_STARTER}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE}
    )
endif()

##############################################################################
# cpu-load-test
##############################################################################

if(NOT ${TESTS_BAT})
    set(TEST_CPU_LOAD_NAME cpu_load_test)

    set(TEST_CPU_LOAD_SERVICE cpu_load_test_service)
    add_executable(${TEST_CPU_LOAD_SERVICE}
        cpu_load_tests/${TEST_CPU_LOAD_SERVICE}.cpp
        cpu_load_tests/cpu_load_measurer.cpp
    )
    target_link_libraries(${TEST_CPU_LOAD_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config files for test into $BUILDDIR/test
    set(TEST_CPU_LOAD_SERVICE_MASTER_CONFIG_FILE ${TEST_CPU_LOAD_NAME}_service_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/cpu_load_tests/conf/${TEST_CPU_LOAD_SERVICE_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/cpu_load_tests/${TEST_CPU_LOAD_SERVICE_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/cpu_load_tests/${TEST_CPU_LOAD_SERVICE_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CPU_LOAD_SERVICE_MASTER_CONFIG_FILE}
        ${TEST_CPU_LOAD_SERVICE}
    )
    set(TEST_CPU_LOAD_SERVICE_SLAVE_CONFIG_FILE ${TEST_CPU_LOAD_NAME}_service_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/cpu_load_tests/conf/${TEST_CPU_LOAD_SERVICE_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/cpu_load_tests/${TEST_CPU_LOAD_SERVICE_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/cpu_load_tests/${TEST_CPU_LOAD_SERVICE_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CPU_LOAD_SERVICE_SLAVE_CONFIG_FILE}
        ${TEST_CPU_LOAD_SERVICE}
    )

    ##############################################################################
    set(TEST_CPU_LOAD_CLIENT cpu_load_test_client)
    add_executable(${TEST_CPU_LOAD_CLIENT}
        cpu_load_tests/${TEST_CPU_LOAD_CLIENT}.cpp
        cpu_load_tests/cpu_load_measurer.cpp
    )
    target_link_libraries(${TEST_CPU_LOAD_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_CPU_LOAD_CLIENT_MASTER_CONFIG_FILE ${TEST_CPU_LOAD_NAME}_client_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/cpu_load_tests/conf/${TEST_CPU_LOAD_CLIENT_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/cpu_load_tests/${TEST_CPU_LOAD_CLIENT_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/cpu_load_tests/${TEST_CPU_LOAD_CLIENT_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CPU_LOAD_CLIENT_MASTER_CONFIG_FILE}
        ${TEST_CPU_LOAD_CLIENT}
    )
    set(TEST_CPU_LOAD_CLIENT_SLAVE_CONFIG_FILE ${TEST_CPU_LOAD_NAME}_client_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/cpu_load_tests/conf/${TEST_CPU_LOAD_CLIENT_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/cpu_load_tests/${TEST_CPU_LOAD_CLIENT_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/cpu_load_tests/${TEST_CPU_LOAD_CLIENT_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CPU_LOAD_CLIENT_SLAVE_CONFIG_FILE}
        ${TEST_CPU_LOAD_CLIENT}
    )

    ##############################################################################
    # copy starter scripts into builddir
    set(TEST_CPU_LOAD_MASTER_STARTER ${TEST_CPU_LOAD_NAME}_master_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/cpu_load_tests/${TEST_CPU_LOAD_MASTER_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CPU_LOAD_MASTER_STARTER}
        ${TEST_CPU_LOAD_SERVICE}
    )
    set(TEST_CPU_LOAD_SLAVE_STARTER ${TEST_CPU_LOAD_NAME}_slave_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/cpu_load_tests/${TEST_CPU_LOAD_SLAVE_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_CPU_LOAD_SLAVE_STARTER}
        ${TEST_CPU_LOAD_SERVICE}
    )
endif()

##############################################################################
# initial event tests
##############################################################################
if(NOT ${TESTS_BAT})
    set(TEST_INITIAL_EVENT_NAME initial_event_test)
    set(TEST_INITIAL_EVENT_SERVICE ${TEST_INITIAL_EVENT_NAME}_service)
    add_executable(${TEST_INITIAL_EVENT_SERVICE} initial_event_tests/${TEST_INITIAL_EVENT_NAME}_service.cpp)
    target_link_libraries(${TEST_INITIAL_EVENT_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_INITIAL_EVENT_CLIENT ${TEST_INITIAL_EVENT_NAME}_client)
    add_executable(${TEST_INITIAL_EVENT_CLIENT} initial_event_tests/${TEST_INITIAL_EVENT_NAME}_client.cpp)
    target_link_libraries(${TEST_INITIAL_EVENT_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_INITIAL_EVENT_AVAILABILITY_CHECKER ${TEST_INITIAL_EVENT_NAME}_availability_checker)
    add_executable(${TEST_INITIAL_EVENT_AVAILABILITY_CHECKER} initial_event_tests/${TEST_INITIAL_EVENT_NAME}_availability_checker.cpp)
    target_link_libraries(${TEST_INITIAL_EVENT_AVAILABILITY_CHECKER}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_INITIAL_EVENT_STOP_SERVICE ${TEST_INITIAL_EVENT_NAME}_stop_service)
    add_executable(${TEST_INITIAL_EVENT_STOP_SERVICE} initial_event_tests/${TEST_INITIAL_EVENT_NAME}_stop_service.cpp)
    target_link_libraries(${TEST_INITIAL_EVENT_STOP_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config files for test into $BUILDDIR/test
    set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    # Copy config files for test into $BUILDDIR/test
    set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_master_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    # Copy config files for test into $BUILDDIR/test
    set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_master_udp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_slave_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_slave_udp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_UDP_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_master_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_slave_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_master_udp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_UDP_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_slave_udp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_UDP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_UDP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_UDP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_UDP_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_partial_same_ports_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_partial_same_ports_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_same_client_ids_same_ports_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_same_client_ids_same_ports_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_same_client_ids_diff_ports_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_same_client_ids_diff_ports_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_same_service_id_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_same_service_id_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    # copy starter scripts into builddir
    if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")
        set(TEST_INITIAL_EVENT_MASTER_STARTER ${TEST_INITIAL_EVENT_NAME}_master_starter_qnx.sh)
    else()
        set(TEST_INITIAL_EVENT_MASTER_STARTER ${TEST_INITIAL_EVENT_NAME}_master_starter.sh)
    endif()
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER}
        ${TEST_INITIAL_EVENT_SERVICE}
    )
    set(TEST_INITIAL_EVENT_SLAVE_STARTER ${TEST_INITIAL_EVENT_NAME}_slave_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/initial_event_tests/${TEST_INITIAL_EVENT_SLAVE_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_SLAVE_STARTER}
        ${TEST_INITIAL_EVENT_SERVICE}
    )
endif()

##############################################################################
# offer tests
##############################################################################
if(NOT ${TESTS_BAT})
    set(TEST_OFFER_NAME offer_test)
    set(TEST_OFFER_SERVICE ${TEST_OFFER_NAME}_service)
    add_executable(${TEST_OFFER_SERVICE} offer_tests/${TEST_OFFER_NAME}_service.cpp)
    target_link_libraries(${TEST_OFFER_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_OFFER_MULTIPLE_OFFERING offer_test_multiple_offerings)
    add_executable(${TEST_OFFER_MULTIPLE_OFFERING} offer_tests/${TEST_OFFER_MULTIPLE_OFFERING}.cpp)
    target_link_libraries(${TEST_OFFER_MULTIPLE_OFFERING}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    # copy starter scripts into builddir
    set(TEST_OFFER_MULTIPLE_OFFERING_STARTER ${TEST_OFFER_MULTIPLE_OFFERING}_starter.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_MULTIPLE_OFFERING_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_OFFER_MULTIPLE_OFFERING_STARTER}
        ${TEST_OFFER_MULTIPLE_OFFERING}
    )

    # Copy config file for local test into $BUILDDIR/test
    set(TEST_OFFER_MULTIPLE_OFFERING_CONFIG ${TEST_OFFER_MULTIPLE_OFFERING}.json)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_MULTIPLE_OFFERING_CONFIG}
        ${NETWORK_TEST_BIN_DIR}/${TEST_OFFER_MULTIPLE_OFFERING_CONFIG}
        ${TEST_OFFER_MULTIPLE_OFFERING}
    )

    set(TEST_OFFER_CLIENT ${TEST_OFFER_NAME}_client)
    add_executable(${TEST_OFFER_CLIENT} offer_tests/${TEST_OFFER_NAME}_client.cpp)
    target_link_libraries(${TEST_OFFER_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_OFFER_SERVICE_EXTERNAL ${TEST_OFFER_NAME}_service_external)
    add_executable(${TEST_OFFER_SERVICE_EXTERNAL} offer_tests/${TEST_OFFER_NAME}_service_external.cpp)
    target_link_libraries(${TEST_OFFER_SERVICE_EXTERNAL}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_OFFER_EXTERNAL_SD_MESSAGE_SENDER ${TEST_OFFER_NAME}_external_sd_msg_sender)
    add_executable(${TEST_OFFER_EXTERNAL_SD_MESSAGE_SENDER} offer_tests/${TEST_OFFER_NAME}_external_sd_msg_sender.cpp)
    target_link_libraries(${TEST_OFFER_EXTERNAL_SD_MESSAGE_SENDER}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # client and service for offer test big sd msg
    set(TEST_OFFER_BIG_NAME offer_test_big_sd_msg)
    set(TEST_OFFER_BIG_SERVICE ${TEST_OFFER_BIG_NAME}_service)
    add_executable(${TEST_OFFER_BIG_SERVICE} offer_tests/${TEST_OFFER_BIG_NAME}_service.cpp)
    target_link_libraries(${TEST_OFFER_BIG_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_OFFER_BIG_CLIENT ${TEST_OFFER_BIG_NAME}_client)
    add_executable(${TEST_OFFER_BIG_CLIENT} offer_tests/${TEST_OFFER_BIG_NAME}_client.cpp)
    target_link_libraries(${TEST_OFFER_BIG_CLIENT}
        ${DLT_LIBRARIES}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        vsomeip_utilities
    )

    # copy starter scripts into builddir
    if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")
        set(TEST_OFFER_LOCAL_STARTER ${TEST_OFFER_NAME}_local_starter_qnx.sh)
    else()
        set(TEST_OFFER_LOCAL_STARTER ${TEST_OFFER_NAME}_local_starter.sh)
    endif()
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_LOCAL_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_OFFER_LOCAL_STARTER}
        ${TEST_OFFER_SERVICE}
    )

    # Copy config file for local test into $BUILDDIR/test
    set(TEST_OFFER_LOCAL_CONFIG_FILE ${TEST_OFFER_NAME}_local.json)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_LOCAL_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_OFFER_LOCAL_CONFIG_FILE}
        ${TEST_OFFER_SERVICE}
    )

    # generate and copy json files into builddir for external test
    set(TEST_OFFER_SLAVE_CONFIG_FILE ${TEST_OFFER_NAME}_external_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/offer_tests/conf/${TEST_OFFER_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_OFFER_SLAVE_CONFIG_FILE}
        ${TEST_OFFER_SERVICE}
    )

    set(TEST_OFFER_MASTER_CONFIG_FILE ${TEST_OFFER_NAME}_external_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/offer_tests/conf/${TEST_OFFER_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_OFFER_MASTER_CONFIG_FILE}
        ${TEST_OFFER_SERVICE}
    )

    # generate and copy json files into builddir for big SD message test
    set(TEST_OFFER_BIG_SLAVE_CONFIG_FILE ${TEST_OFFER_BIG_NAME}_slave.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/offer_tests/conf/${TEST_OFFER_BIG_SLAVE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_BIG_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_BIG_SLAVE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_OFFER_BIG_SLAVE_CONFIG_FILE}
        ${TEST_OFFER_BIG_SERVICE}
    )

    set(TEST_OFFER_BIG_MASTER_CONFIG_FILE ${TEST_OFFER_BIG_NAME}_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/offer_tests/conf/${TEST_OFFER_BIG_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_BIG_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_BIG_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_OFFER_BIG_MASTER_CONFIG_FILE}
        ${TEST_OFFER_BIG_SERVICE}
    )

    # Copy starter scripts for external test to $BUILDDIR/test
    if (${CMAKE_SYSTEM_NAME} MATCHES "QNX")
        set(TEST_OFFER_EXTERNAL_MASTER_STARTER ${TEST_OFFER_NAME}_external_master_starter_qnx.sh)
    else()
        set(TEST_OFFER_EXTERNAL_MASTER_STARTER ${TEST_OFFER_NAME}_external_master_starter.sh)
    endif()
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/offer_tests/conf/${TEST_OFFER_EXTERNAL_MASTER_STARTER}.in
        ${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_EXTERNAL_MASTER_STARTER}
        @ONLY)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_EXTERNAL_MASTER_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_OFFER_EXTERNAL_MASTER_STARTER}
        ${TEST_OFFER_SERVICE}
    )
    set(TEST_OFFER_EXTERNAL_SLAVE_STARTER ${TEST_OFFER_NAME}_external_slave_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_EXTERNAL_SLAVE_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_OFFER_EXTERNAL_SLAVE_STARTER}
        ${TEST_OFFER_SERVICE}
    )

    # Copy starter scripts for external test to $BUILDDIR/test
    set(TEST_OFFER_BIG_MASTER_STARTER ${TEST_OFFER_BIG_NAME}_master_starter.sh)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/offer_tests/conf/${TEST_OFFER_BIG_MASTER_STARTER}.in
        ${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_BIG_MASTER_STARTER}
        @ONLY)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_BIG_MASTER_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_OFFER_BIG_MASTER_STARTER}
        ${TEST_OFFER_BIG_SERVICE}
    )
    # Copy starter scripts for external test to $BUILDDIR/test
    set(TEST_OFFER_BIG_EXTERNAL_SLAVE_STARTER ${TEST_OFFER_BIG_NAME}_slave_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/offer_tests/${TEST_OFFER_BIG_EXTERNAL_SLAVE_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_OFFER_BIG_EXTERNAL_SLAVE_STARTER}
        ${TEST_OFFER_BIG_SERVICE}
    )
endif()

##############################################################################
# offered services info tests
##############################################################################
if(NOT ${TESTS_BAT})
    set(TEST_OFFERED_SERVICES_INFO_NAME offered_services_info_test)
    set(TEST_OFFERED_SERVICES_INFO_SERVICE ${TEST_OFFERED_SERVICES_INFO_NAME}_service)
    add_executable(${TEST_OFFERED_SERVICES_INFO_SERVICE} offered_services_info_test/${TEST_OFFERED_SERVICES_INFO_NAME}_service.cpp)
    target_link_libraries(${TEST_OFFERED_SERVICES_INFO_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_OFFERED_SERVICES_INFO_CLIENT ${TEST_OFFERED_SERVICES_INFO_NAME}_client)
    add_executable(${TEST_OFFERED_SERVICES_INFO_CLIENT} offered_services_info_test/${TEST_OFFERED_SERVICES_INFO_NAME}_client.cpp)
    target_link_libraries(${TEST_OFFERED_SERVICES_INFO_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # copy starter scripts into builddir
    set(TEST_OFFERED_SERVICES_INFO_LOCAL_STARTER ${TEST_OFFERED_SERVICES_INFO_NAME}_local_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/offered_services_info_test/${TEST_OFFERED_SERVICES_INFO_LOCAL_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_OFFERED_SERVICES_INFO_LOCAL_STARTER}
        ${TEST_OFFERED_SERVICES_INFO_SERVICE}
    )

    # Copy config file for local test into $BUILDDIR/test
    set(TEST_OFFERED_SERVICES_INFO_LOCAL_CONFIG_FILE ${TEST_OFFERED_SERVICES_INFO_NAME}_local.json)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/offered_services_info_test/${TEST_OFFERED_SERVICES_INFO_LOCAL_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_OFFERED_SERVICES_INFO_LOCAL_CONFIG_FILE}
        ${TEST_OFFERED_SERVICES_INFO_SERVICE}
    )
endif()

##############################################################################
# pending subscription tests tests
##############################################################################
if(NOT ${TESTS_BAT})
    set(TEST_PENDING_SUBSCRIPTION_NAME pending_subscription_test)
    set(TEST_PENDING_SUBSCRIPTION_SERVICE ${TEST_PENDING_SUBSCRIPTION_NAME}_service)
    add_executable(${TEST_PENDING_SUBSCRIPTION_SERVICE} pending_subscription_tests/${TEST_PENDING_SUBSCRIPTION_NAME}_service.cpp)
    target_link_libraries(${TEST_PENDING_SUBSCRIPTION_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    file(GLOB sd_sources
        "../../implementation/service_discovery/src/*entry*.cpp"
        "../../implementation/service_discovery/src/*option*.cpp"
        "../../implementation/service_discovery/src/*message*.cpp"
    )
    set(TEST_PENDING_SUBSCRIPTION_CLIENT ${TEST_PENDING_SUBSCRIPTION_NAME}_sd_msg_sender)
    add_executable(${TEST_PENDING_SUBSCRIPTION_CLIENT}
        pending_subscription_tests/${TEST_PENDING_SUBSCRIPTION_NAME}_sd_msg_sender.cpp
        ${PROJECT_SOURCE_DIR}/implementation/message/src/deserializer.cpp
        ${PROJECT_SOURCE_DIR}/implementation/message/src/message_impl.cpp
        ${PROJECT_SOURCE_DIR}/implementation/message/src/payload_impl.cpp
        ${sd_sources}
    )

    target_link_libraries(${TEST_PENDING_SUBSCRIPTION_CLIENT}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-sd
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # copy starter scripts into builddir
    set(TEST_PENDING_SUBSCRIPTION_MASTER_STARTER ${TEST_PENDING_SUBSCRIPTION_NAME}_master_starter.sh)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/pending_subscription_tests/conf/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER}.in
        ${NETWORK_TEST_SRC_DIR}/pending_subscription_tests/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER}
        @ONLY)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/pending_subscription_tests/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER}
        ${TEST_PENDING_SUBSCRIPTION_SERVICE}
    )

    # Copy config file for local test into $BUILDDIR/test
    set(TEST_PENDING_SUBSCRIPTION_CONFIG_FILE ${TEST_PENDING_SUBSCRIPTION_NAME}_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/pending_subscription_tests/conf/${TEST_PENDING_SUBSCRIPTION_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/pending_subscription_tests/${TEST_PENDING_SUBSCRIPTION_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/pending_subscription_tests/${TEST_PENDING_SUBSCRIPTION_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_PENDING_SUBSCRIPTION_CONFIG_FILE}
        ${TEST_PENDING_SUBSCRIPTION_SERVICE}
    )
endif()

##############################################################################
# malicious data tests
##############################################################################
if(NOT ${TESTS_BAT})
    set(TEST_MALICIOUS_DATA_NAME malicious_data_test)
    set(TEST_MALICIOUS_DATA_SERVICE ${TEST_MALICIOUS_DATA_NAME}_service)
    add_executable(${TEST_MALICIOUS_DATA_SERVICE} malicious_data_tests/${TEST_MALICIOUS_DATA_NAME}_service.cpp)
    target_link_libraries(${TEST_MALICIOUS_DATA_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    file(GLOB sd_sources
        "../../implementation/service_discovery/src/*entry*.cpp"
        "../../implementation/service_discovery/src/*option*.cpp"
        "../../implementation/service_discovery/src/*message*.cpp"
    )
    set(TEST_MALICIOUS_DATA_CLIENT ${TEST_MALICIOUS_DATA_NAME}_msg_sender)
    add_executable(${TEST_MALICIOUS_DATA_CLIENT}
        malicious_data_tests/${TEST_MALICIOUS_DATA_CLIENT}.cpp
        ${PROJECT_SOURCE_DIR}/implementation/message/src/deserializer.cpp
        ${PROJECT_SOURCE_DIR}/implementation/message/src/message_impl.cpp
        ${PROJECT_SOURCE_DIR}/implementation/message/src/payload_impl.cpp
        ${sd_sources}
    )

    target_link_libraries(${TEST_MALICIOUS_DATA_CLIENT}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-sd
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # copy starter scripts into builddir
    set(TEST_MALICIOUS_DATA_MASTER_STARTER ${TEST_MALICIOUS_DATA_NAME}_master_starter.sh)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/malicious_data_tests/conf/${TEST_MALICIOUS_DATA_MASTER_STARTER}.in
        ${NETWORK_TEST_SRC_DIR}/malicious_data_tests/${TEST_MALICIOUS_DATA_MASTER_STARTER}
        @ONLY)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/malicious_data_tests/${TEST_MALICIOUS_DATA_MASTER_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_MALICIOUS_DATA_MASTER_STARTER}
        ${TEST_MALICIOUS_DATA_SERVICE}
    )

    # Copy config file for local test into $BUILDDIR/test
    set(TEST_MALICIOUS_DATA_CONFIG_FILE ${TEST_MALICIOUS_DATA_NAME}_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/malicious_data_tests/conf/${TEST_MALICIOUS_DATA_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/malicious_data_tests/${TEST_MALICIOUS_DATA_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/malicious_data_tests/${TEST_MALICIOUS_DATA_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_MALICIOUS_DATA_CONFIG_FILE}
        ${TEST_MALICIOUS_DATA_SERVICE}
    )
endif()

##############################################################################
# e2e test
##############################################################################

if(NOT ${TESTS_BAT})
    set(TEST_E2E_NAME e2e_test)
    set(TEST_E2E_SERVICE e2e_test_service)
    set(TEST_E2E_CLIENT e2e_test_client)

    add_executable(${TEST_E2E_SERVICE} e2e_tests/${TEST_E2E_SERVICE}.cpp)
    target_link_libraries(${TEST_E2E_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    add_executable(${TEST_E2E_CLIENT}
        e2e_tests/${TEST_E2E_CLIENT}.cpp
    )
    target_link_libraries(${TEST_E2E_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy service config file for external allow tests into $BUILDDIR/test
    set(TEST_E2E_SERVICE_CONFIG_FILE_EXTERNAL ${TEST_E2E_NAME}_service_external.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/conf/${TEST_E2E_SERVICE_CONFIG_FILE_EXTERNAL}.in
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_SERVICE_CONFIG_FILE_EXTERNAL}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_SERVICE_CONFIG_FILE_EXTERNAL}
        ${NETWORK_TEST_BIN_DIR}/${TEST_E2E_SERVICE_CONFIG_FILE_EXTERNAL}
        ${TEST_E2E_SERVICE}
    )

    # Copy client config file for external allow tests into $BUILDDIR/test
    set(TEST_E2E_CLIENT_CONFIG_FILE_EXTERNAL ${TEST_E2E_NAME}_client_external.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/conf/${TEST_E2E_CLIENT_CONFIG_FILE_EXTERNAL}.in
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_CLIENT_CONFIG_FILE_EXTERNAL}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_CLIENT_CONFIG_FILE_EXTERNAL}
        ${NETWORK_TEST_BIN_DIR}/${TEST_E2E_CLIENT_CONFIG_FILE_EXTERNAL}
        ${TEST_E2E_SERVICE}
    )

    # Copy bashscript to start external tests (master) into $BUILDDIR/test
    set(TEST_E2E_EXTERNAL_MASTER_START_SCRIPT ${TEST_E2E_NAME}_external_master_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_EXTERNAL_MASTER_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_E2E_EXTERNAL_MASTER_START_SCRIPT}
        ${TEST_E2E_SERVICE}
    )

    # Copy bashscript to start external tests (slave) into $BUILDDIR/test
    set(TEST_E2E_EXTERNAL_SLAVE_START_SCRIPT ${TEST_E2E_NAME}_external_slave_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_EXTERNAL_SLAVE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_E2E_EXTERNAL_SLAVE_START_SCRIPT}
        ${TEST_E2E_SERVICE}
    )
endif()

##############################################################################
# e2e profile 04 test
##############################################################################

if(NOT ${TESTS_BAT} AND ${TEST_E2E_PROFILE_04})
    set(TEST_E2E_PROFILE_04_NAME e2e_profile_04_test)
    set(TEST_E2E_PROFILE_04_SERVICE e2e_profile_04_test_service)
    set(TEST_E2E_PROFILE_04_CLIENT e2e_profile_04_test_client)

    add_executable(${TEST_E2E_PROFILE_04_SERVICE} e2e_tests/${TEST_E2E_PROFILE_04_SERVICE}.cpp)
    target_link_libraries(${TEST_E2E_PROFILE_04_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    add_executable(${TEST_E2E_PROFILE_04_CLIENT}
        e2e_tests/${TEST_E2E_PROFILE_04_CLIENT}.cpp
    )
    target_link_libraries(${TEST_E2E_PROFILE_04_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy service config file for external allow tests into $BUILDDIR/test
    set(TEST_E2E_PROFILE_04_SERVICE_CONFIG_FILE_EXTERNAL ${TEST_E2E_PROFILE_04_NAME}_service_external.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/conf/${TEST_E2E_PROFILE_04_SERVICE_CONFIG_FILE_EXTERNAL}.in
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_PROFILE_04_SERVICE_CONFIG_FILE_EXTERNAL}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_PROFILE_04_SERVICE_CONFIG_FILE_EXTERNAL}
        ${NETWORK_TEST_BIN_DIR}/${TEST_E2E_PROFILE_04_SERVICE_CONFIG_FILE_EXTERNAL}
        ${TEST_E2E_PROFILE_04_SERVICE}
    )

    # Copy client config file for external allow tests into $BUILDDIR/test
    set(TEST_E2E_PROFILE_04_CLIENT_CONFIG_FILE_EXTERNAL ${TEST_E2E_PROFILE_04_NAME}_client_external.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/conf/${TEST_E2E_PROFILE_04_CLIENT_CONFIG_FILE_EXTERNAL}.in
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_PROFILE_04_CLIENT_CONFIG_FILE_EXTERNAL}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_PROFILE_04_CLIENT_CONFIG_FILE_EXTERNAL}
        ${NETWORK_TEST_BIN_DIR}/${TEST_E2E_PROFILE_04_CLIENT_CONFIG_FILE_EXTERNAL}
        ${TEST_E2E_PROFILE_04_SERVICE}
    )

    # Copy bashscript to start external tests (master) into $BUILDDIR/test
    set(TEST_E2E_PROFILE_04_EXTERNAL_MASTER_START_SCRIPT ${TEST_E2E_PROFILE_04_NAME}_external_master_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_PROFILE_04_EXTERNAL_MASTER_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_E2E_PROFILE_04_EXTERNAL_MASTER_START_SCRIPT}
        ${TEST_E2E_PROFILE_04_SERVICE}
    )

    # Copy bashscript to start external tests (slave) into $BUILDDIR/test
    set(TEST_E2E_PROFILE_04_EXTERNAL_SLAVE_START_SCRIPT ${TEST_E2E_PROFILE_04_NAME}_external_slave_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_PROFILE_04_EXTERNAL_SLAVE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_E2E_PROFILE_04_EXTERNAL_SLAVE_START_SCRIPT}
        ${TEST_E2E_PROFILE_04_SERVICE}
    )
endif()

##############################################################################
# e2e profile 07 test
##############################################################################

if(NOT ${TESTS_BAT} AND ${TEST_E2E_PROFILE_07})
    set(TEST_E2E_PROFILE_07_NAME e2e_profile_07_test)
    set(TEST_E2E_PROFILE_07_SERVICE e2e_profile_07_test_service)
    set(TEST_E2E_PROFILE_07_CLIENT e2e_profile_07_test_client)

    add_executable(${TEST_E2E_PROFILE_07_SERVICE} e2e_tests/${TEST_E2E_PROFILE_07_SERVICE}.cpp)
    target_link_libraries(${TEST_E2E_PROFILE_07_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    add_executable(${TEST_E2E_PROFILE_07_CLIENT}
        e2e_tests/${TEST_E2E_PROFILE_07_CLIENT}.cpp
    )
    target_link_libraries(${TEST_E2E_PROFILE_07_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy service config file for external allow tests into $BUILDDIR/test
    set(TEST_E2E_PROFILE_07_SERVICE_CONFIG_FILE_EXTERNAL ${TEST_E2E_PROFILE_07_NAME}_service_external.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/conf/${TEST_E2E_PROFILE_07_SERVICE_CONFIG_FILE_EXTERNAL}.in
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_PROFILE_07_SERVICE_CONFIG_FILE_EXTERNAL}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_PROFILE_07_SERVICE_CONFIG_FILE_EXTERNAL}
        ${NETWORK_TEST_BIN_DIR}/${TEST_E2E_PROFILE_07_SERVICE_CONFIG_FILE_EXTERNAL}
        ${TEST_E2E_PROFILE_07_SERVICE}
    )

    # Copy client config file for external allow tests into $BUILDDIR/test
    set(TEST_E2E_PROFILE_07_CLIENT_CONFIG_FILE_EXTERNAL ${TEST_E2E_PROFILE_07_NAME}_client_external.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/conf/${TEST_E2E_PROFILE_07_CLIENT_CONFIG_FILE_EXTERNAL}.in
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_PROFILE_07_CLIENT_CONFIG_FILE_EXTERNAL}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_PROFILE_07_CLIENT_CONFIG_FILE_EXTERNAL}
        ${NETWORK_TEST_BIN_DIR}/${TEST_E2E_PROFILE_07_CLIENT_CONFIG_FILE_EXTERNAL}
        ${TEST_E2E_PROFILE_07_SERVICE}
    )

    # Copy bashscript to start external tests (master) into $BUILDDIR/test
    set(TEST_E2E_PROFILE_07_EXTERNAL_MASTER_START_SCRIPT ${TEST_E2E_PROFILE_07_NAME}_external_master_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_PROFILE_07_EXTERNAL_MASTER_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_E2E_PROFILE_07_EXTERNAL_MASTER_START_SCRIPT}
        ${TEST_E2E_PROFILE_07_SERVICE}
    )

    # Copy bashscript to start external tests (slave) into $BUILDDIR/test
    set(TEST_E2E_PROFILE_07_EXTERNAL_SLAVE_START_SCRIPT ${TEST_E2E_PROFILE_07_NAME}_external_slave_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/e2e_tests/${TEST_E2E_PROFILE_07_EXTERNAL_SLAVE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_E2E_PROFILE_07_EXTERNAL_SLAVE_START_SCRIPT}
        ${TEST_E2E_PROFILE_07_SERVICE}
    )
endif()

##############################################################################
# event tests
##############################################################################

if(NOT ${TESTS_BAT})
    set(TEST_EVENT_NAME event_test)
    set(TEST_EVENT_SERVICE ${TEST_EVENT_NAME}_service)
    set(TEST_EVENT_CLIENT ${TEST_EVENT_NAME}_client)

    add_executable(${TEST_EVENT_SERVICE} event_tests/${TEST_EVENT_SERVICE}.cpp)
    target_link_libraries(${TEST_EVENT_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    add_executable(${TEST_EVENT_CLIENT}
        event_tests/${TEST_EVENT_CLIENT}.cpp
    )
    target_link_libraries(${TEST_EVENT_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )
    # Copy service config file for external allow tests into $BUILDDIR/test
    set(TEST_EVENT_SLAVE_TCP_CONFIG_FILE ${TEST_EVENT_NAME}_slave_tcp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/event_tests/conf/${TEST_EVENT_SLAVE_TCP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/event_tests/${TEST_EVENT_SLAVE_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/event_tests/${TEST_EVENT_SLAVE_TCP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_EVENT_SLAVE_TCP_CONFIG_FILE}
        ${TEST_EVENT_SERVICE}
    )

    set(TEST_EVENT_SLAVE_UDP_CONFIG_FILE ${TEST_EVENT_NAME}_slave_udp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/event_tests/conf/${TEST_EVENT_SLAVE_UDP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/event_tests/${TEST_EVENT_SLAVE_UDP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/event_tests/${TEST_EVENT_SLAVE_UDP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_EVENT_SLAVE_UDP_CONFIG_FILE}
        ${TEST_EVENT_SERVICE}
    )

    set(TEST_EVENT_MASTER_CONFIG_FILE ${TEST_EVENT_NAME}_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/event_tests/conf/${TEST_EVENT_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/event_tests/${TEST_EVENT_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/event_tests/${TEST_EVENT_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_EVENT_MASTER_CONFIG_FILE}
        ${TEST_EVENT_CLIENT}
    )

    # Copy bashscript to start test (master) into $BUILDDIR/test
    set(TEST_EVENT_MASTER_START_SCRIPT ${TEST_EVENT_NAME}_master_starter.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/event_tests/${TEST_EVENT_MASTER_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_EVENT_MASTER_START_SCRIPT}
        ${TEST_EVENT_SERVICE}
    )

    # Copy bashscript to start external tests (slave) into $BUILDDIR/test
    set(TEST_EVENT_SLAVE_START_SCRIPT ${TEST_EVENT_NAME}_slave_starter.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/event_tests/${TEST_EVENT_SLAVE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_EVENT_SLAVE_START_SCRIPT}
        ${TEST_EVENT_CLIENT}
    )

endif()

##############################################################################
# npdu-test
##############################################################################
if(NOT ${TESTS_BAT} AND NOT ${CMAKE_SYSTEM_NAME} MATCHES "QNX")
    set(TEST_NPDU_NAME npdu_test)

    set(TEST_NPDU_DAEMON npdu_test_rmd)
    set(TEST_NPDU_DAEMON_CLIENT ${TEST_NPDU_DAEMON}_client_side)
    add_executable(${TEST_NPDU_DAEMON_CLIENT} npdu_tests/${TEST_NPDU_DAEMON}.cpp)
    set_target_properties(${TEST_NPDU_DAEMON_CLIENT} PROPERTIES COMPILE_FLAGS -DRMD_CLIENT_SIDE)
    target_link_libraries(${TEST_NPDU_DAEMON_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_NPDU_DAEMON_SERVICE ${TEST_NPDU_DAEMON}_service_side)
    add_executable(${TEST_NPDU_DAEMON_SERVICE} npdu_tests/${TEST_NPDU_DAEMON}.cpp)
    set_target_properties(${TEST_NPDU_DAEMON_SERVICE} PROPERTIES COMPILE_FLAGS -DRMD_SERVICE_SIDE)
    target_link_libraries(${TEST_NPDU_DAEMON_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    ##############################################################################

    set(TEST_NPDU_SERVICE npdu_test_service)

    set(TEST_NPDU_SERVICE_ONE npdu_test_service_1)
    add_executable(${TEST_NPDU_SERVICE_ONE} npdu_tests/${TEST_NPDU_SERVICE}.cpp)
    set_target_properties(${TEST_NPDU_SERVICE_ONE} PROPERTIES COMPILE_FLAGS -DSERVICE_NUMBER=0)
    target_link_libraries(${TEST_NPDU_SERVICE_ONE}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-cfg
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_NPDU_SERVICE_TWO npdu_test_service_2)
    add_executable(${TEST_NPDU_SERVICE_TWO} npdu_tests/${TEST_NPDU_SERVICE}.cpp)
    set_target_properties(${TEST_NPDU_SERVICE_TWO} PROPERTIES COMPILE_FLAGS -DSERVICE_NUMBER=1)
    target_link_libraries(${TEST_NPDU_SERVICE_TWO}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-cfg
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_NPDU_SERVICE_THREE npdu_test_service_3)
    add_executable(${TEST_NPDU_SERVICE_THREE} npdu_tests/${TEST_NPDU_SERVICE}.cpp)
    set_target_properties(${TEST_NPDU_SERVICE_THREE} PROPERTIES COMPILE_FLAGS -DSERVICE_NUMBER=2)
    target_link_libraries(${TEST_NPDU_SERVICE_THREE}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-cfg
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_NPDU_SERVICE_FOUR npdu_test_service_4)
    add_executable(${TEST_NPDU_SERVICE_FOUR} npdu_tests/${TEST_NPDU_SERVICE}.cpp)
    set_target_properties(${TEST_NPDU_SERVICE_FOUR} PROPERTIES COMPILE_FLAGS -DSERVICE_NUMBER=3)
    target_link_libraries(${TEST_NPDU_SERVICE_FOUR}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-cfg
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config file for service w/o npdu into $BUILDDIR/test
    set(TEST_NPDU_SERVICE_CONFIG_FILE ${TEST_NPDU_SERVICE}_no_npdu.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/npdu_tests/conf/${TEST_NPDU_SERVICE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/npdu_tests/${TEST_NPDU_SERVICE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/npdu_tests/${TEST_NPDU_SERVICE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_NPDU_SERVICE_CONFIG_FILE}
        ${TEST_NPDU_SERVICE_ONE}
    )

    # Copy bashscript to start service w/o npdu into $BUILDDIR/test
    set(TEST_NPDU_SERVICE_START_SCRIPT ${TEST_NPDU_SERVICE}_no_npdu_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/npdu_tests/${TEST_NPDU_SERVICE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_NPDU_SERVICE_START_SCRIPT}
        ${TEST_NPDU_SERVICE_ONE}
    )

    # Copy config file for service with npdu into $BUILDDIR/test
    set(TEST_NPDU_SERVICE_CONFIG_FILE ${TEST_NPDU_SERVICE}_npdu.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/npdu_tests/conf/${TEST_NPDU_SERVICE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/npdu_tests/${TEST_NPDU_SERVICE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/npdu_tests/${TEST_NPDU_SERVICE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_NPDU_SERVICE_CONFIG_FILE}
        ${TEST_NPDU_SERVICE_ONE}
    )

    # Copy bashscript to start service with npdu into $BUILDDIR/test
    set(TEST_NPDU_SERVICE_START_SCRIPT ${TEST_NPDU_SERVICE}_npdu_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/npdu_tests/${TEST_NPDU_SERVICE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_NPDU_SERVICE_START_SCRIPT}
        ${TEST_NPDU_SERVICE_ONE}
    )

    ##############################################################################

    set(TEST_NPDU_CLIENT npdu_test_client)

    set(TEST_NPDU_CLIENT_ONE npdu_test_client_1)
    add_executable(${TEST_NPDU_CLIENT_ONE} npdu_tests/${TEST_NPDU_CLIENT}.cpp)
    target_link_libraries(${TEST_NPDU_CLIENT_ONE}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-cfg
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_NPDU_CLIENT_TWO npdu_test_client_2)
    add_executable(${TEST_NPDU_CLIENT_TWO} npdu_tests/${TEST_NPDU_CLIENT}.cpp)
    target_link_libraries(${TEST_NPDU_CLIENT_TWO}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-cfg
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_NPDU_CLIENT_THREE npdu_test_client_3)
    add_executable(${TEST_NPDU_CLIENT_THREE} npdu_tests/${TEST_NPDU_CLIENT}.cpp)
    target_link_libraries(${TEST_NPDU_CLIENT_THREE}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-cfg
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_NPDU_CLIENT_FOUR npdu_test_client_4)
    add_executable(${TEST_NPDU_CLIENT_FOUR} npdu_tests/${TEST_NPDU_CLIENT}.cpp)
    target_link_libraries(${TEST_NPDU_CLIENT_FOUR}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-cfg
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    # Copy config files for client w/o npdu into $BUILDDIR/test
    set(TEST_NPDU_CLIENT_CONFIG_FILE ${TEST_NPDU_CLIENT}_no_npdu.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/npdu_tests/conf/${TEST_NPDU_CLIENT_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/npdu_tests/${TEST_NPDU_CLIENT_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/npdu_tests/${TEST_NPDU_CLIENT_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_NPDU_CLIENT_CONFIG_FILE}
        ${TEST_NPDU_CLIENT_ONE}
    )

    # Copy bashscript to start client w/o npdu into $BUILDDIR/test
    set(TEST_NPDU_CLIENT_START_SCRIPT ${TEST_NPDU_CLIENT}_no_npdu_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/npdu_tests/${TEST_NPDU_CLIENT_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_NPDU_CLIENT_START_SCRIPT}
        ${TEST_NPDU_CLIENT_ONE}
    )

    # Copy config file for client with npdu into $BUILDDIR/test
    set(TEST_NPDU_CLIENT_CONFIG_FILE ${TEST_NPDU_CLIENT}_npdu.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/npdu_tests/conf/${TEST_NPDU_CLIENT_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/npdu_tests/${TEST_NPDU_CLIENT_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/npdu_tests/${TEST_NPDU_CLIENT_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_NPDU_CLIENT_CONFIG_FILE}
        ${TEST_NPDU_CLIENT_ONE}
    )

    # Copy bashscript to start client with npdu into $BUILDDIR/test
    set(TEST_NPDU_CLIENT_START_SCRIPT ${TEST_NPDU_CLIENT}_npdu_start.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/npdu_tests/${TEST_NPDU_CLIENT_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_NPDU_CLIENT_START_SCRIPT}
        ${TEST_NPDU_CLIENT_ONE}
    )

    ##############################################################################

    set(TEST_NPDU_STARTER ${TEST_NPDU_NAME}_starter.sh)
    copy_to_builddir(${NETWORK_TEST_SRC_DIR}/npdu_tests/${TEST_NPDU_STARTER}
        ${NETWORK_TEST_BIN_DIR}/${TEST_NPDU_STARTER}
        ${TEST_NPDU_DAEMON_CLIENT}
    )
endif()

##############################################################################
# someip tp tests
##############################################################################

if(NOT ${TESTS_BAT})
    set(TEST_SOMEIPTP_NAME someip_tp_test)
    set(TEST_SOMEIPTP_SERVICE ${TEST_SOMEIPTP_NAME}_service)

    add_executable(${TEST_SOMEIPTP_SERVICE} someip_tp_tests/${TEST_SOMEIPTP_SERVICE}.cpp)
    target_link_libraries(${TEST_SOMEIPTP_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    file(GLOB sd_sources
        "../../implementation/service_discovery/src/*entry*.cpp"
        "../../implementation/service_discovery/src/*option*.cpp"
        "../../implementation/service_discovery/src/*message*.cpp"
    )
    set(TEST_SOMEIPTP_CLIENT ${TEST_SOMEIPTP_NAME}_msg_sender)
    add_executable(${TEST_SOMEIPTP_CLIENT}
        someip_tp_tests/${TEST_SOMEIPTP_CLIENT}.cpp
        ${PROJECT_SOURCE_DIR}/implementation/message/src/deserializer.cpp
        ${PROJECT_SOURCE_DIR}/implementation/message/src/message_impl.cpp
        ${PROJECT_SOURCE_DIR}/implementation/message/src/payload_impl.cpp
        ${PROJECT_SOURCE_DIR}/implementation/endpoints/src/tp.cpp
        ${PROJECT_SOURCE_DIR}/implementation/endpoints/src/tp_reassembler.cpp
        ${PROJECT_SOURCE_DIR}/implementation/endpoints/src/tp_message.cpp
        ${sd_sources}
    )

    target_link_libraries(${TEST_SOMEIPTP_CLIENT}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        ${VSOMEIP_NAME}
        ${VSOMEIP_NAME}-sd
        vsomeip_utilities
    )

    # Copy service config file for external allow tests into $BUILDDIR/test
    set(TEST_SOMEIPTP_MASTER_CONFIG_FILE ${TEST_SOMEIPTP_NAME}_master.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/someip_tp_tests/conf/${TEST_SOMEIPTP_MASTER_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/someip_tp_tests/${TEST_SOMEIPTP_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/someip_tp_tests/${TEST_SOMEIPTP_MASTER_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SOMEIPTP_MASTER_CONFIG_FILE}
        ${TEST_SOMEIPTP_CLIENT}
    )

    # Copy bashscript to start test (master) into $BUILDDIR/test
    set(TEST_SOMEIPTP_MASTER_START_SCRIPT ${TEST_SOMEIPTP_NAME}_master_starter.sh)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/someip_tp_tests/conf/${TEST_SOMEIPTP_MASTER_START_SCRIPT}.in
        ${NETWORK_TEST_SRC_DIR}/someip_tp_tests/${TEST_SOMEIPTP_MASTER_START_SCRIPT}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/someip_tp_tests/${TEST_SOMEIPTP_MASTER_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SOMEIPTP_MASTER_START_SCRIPT}
        ${TEST_SOMEIPTP_SERVICE}
    )
endif()

##############################################################################
# second IP address tests
##############################################################################

if(NOT ${TESTS_BAT} AND ${TEST_SECOND_ADDRESS})
    set(TEST_SECOND_ADDRESS_NAME second_address_test)
    set(TEST_SECOND_ADDRESS_SERVICE ${TEST_SECOND_ADDRESS_NAME}_service)
    set(TEST_SECOND_ADDRESS_CLIENT ${TEST_SECOND_ADDRESS_NAME}_client)

    add_executable(${TEST_SECOND_ADDRESS_SERVICE}
        second_address_tests/${TEST_SECOND_ADDRESS_SERVICE}.cpp
    )
    target_link_libraries(${TEST_SECOND_ADDRESS_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    add_executable(${TEST_SECOND_ADDRESS_CLIENT}
        second_address_tests/${TEST_SECOND_ADDRESS_CLIENT}.cpp
    )
    target_link_libraries(${TEST_SECOND_ADDRESS_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_SECOND_ADDRESS_MASTER_SERVICE_UDP_CONFIG_FILE ${TEST_SECOND_ADDRESS_NAME}_master_service_udp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/second_address_tests/conf/${TEST_SECOND_ADDRESS_MASTER_SERVICE_UDP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/second_address_tests/${TEST_SECOND_ADDRESS_MASTER_SERVICE_UDP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/second_address_tests/${TEST_SECOND_ADDRESS_MASTER_SERVICE_UDP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SECOND_ADDRESS_MASTER_SERVICE_UDP_CONFIG_FILE}
        ${TEST_SECOND_ADDRESS_SERVICE}
    )

    set(TEST_SECOND_ADDRESS_SLAVE_CLIENT_CONFIG_FILE ${TEST_SECOND_ADDRESS_NAME}_slave_client.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/second_address_tests/conf/${TEST_SECOND_ADDRESS_SLAVE_CLIENT_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/second_address_tests/${TEST_SECOND_ADDRESS_SLAVE_CLIENT_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/second_address_tests/${TEST_SECOND_ADDRESS_SLAVE_CLIENT_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SECOND_ADDRESS_SLAVE_CLIENT_CONFIG_FILE}
        ${TEST_SECOND_ADDRESS_CLIENT}
    )

    set(TEST_SECOND_ADDRESS_MASTER_CLIENT_CONFIG_FILE ${TEST_SECOND_ADDRESS_NAME}_master_client.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/second_address_tests/conf/${TEST_SECOND_ADDRESS_MASTER_CLIENT_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/second_address_tests/${TEST_SECOND_ADDRESS_MASTER_CLIENT_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/second_address_tests/${TEST_SECOND_ADDRESS_MASTER_CLIENT_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SECOND_ADDRESS_MASTER_CLIENT_CONFIG_FILE}
        ${TEST_SECOND_ADDRESS_CLIENT}
    )

    set(TEST_SECOND_ADDRESS_SLAVE_SERVICE_UDP_CONFIG_FILE ${TEST_SECOND_ADDRESS_NAME}_slave_service_udp.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/second_address_tests/conf/${TEST_SECOND_ADDRESS_SLAVE_SERVICE_UDP_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/second_address_tests/${TEST_SECOND_ADDRESS_SLAVE_SERVICE_UDP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/second_address_tests/${TEST_SECOND_ADDRESS_SLAVE_SERVICE_UDP_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SECOND_ADDRESS_SLAVE_SERVICE_UDP_CONFIG_FILE}
        ${TEST_SECOND_ADDRESS_SERVICE}
    )

    set(TEST_SECOND_ADDRESS_MASTER_START_SCRIPT ${TEST_SECOND_ADDRESS_NAME}_master_starter.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/second_address_tests/${TEST_SECOND_ADDRESS_MASTER_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SECOND_ADDRESS_MASTER_START_SCRIPT}
        ${TEST_SECOND_ADDRESS_SERVICE}
    )

    set(TEST_SECOND_ADDRESS_SLAVE_START_SCRIPT ${TEST_SECOND_ADDRESS_NAME}_slave_starter.sh)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/second_address_tests/conf/${TEST_SECOND_ADDRESS_SLAVE_START_SCRIPT}.in
        ${NETWORK_TEST_SRC_DIR}/second_address_tests/${TEST_SECOND_ADDRESS_SLAVE_START_SCRIPT}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/second_address_tests/${TEST_SECOND_ADDRESS_SLAVE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SECOND_ADDRESS_SLAVE_START_SCRIPT}
        ${TEST_SECOND_ADDRESS_CLIENT}
    )
endif()

##############################################################################
# suspend resume tests
##############################################################################
if(NOT ${TESTS_BAT})

    set(TEST_SUSPEND_RESUME_NAME suspend_resume_test)
    set(TEST_SUSPEND_RESUME_SERVICE ${TEST_SUSPEND_RESUME_NAME}_service)
    set(TEST_SUSPEND_RESUME_CLIENT ${TEST_SUSPEND_RESUME_NAME}_client)

    add_executable(${TEST_SUSPEND_RESUME_SERVICE}
        suspend_resume_tests/${TEST_SUSPEND_RESUME_SERVICE}.cpp
    )
    target_link_libraries(${TEST_SUSPEND_RESUME_SERVICE}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    add_executable(${TEST_SUSPEND_RESUME_CLIENT}
        suspend_resume_tests/${TEST_SUSPEND_RESUME_CLIENT}.cpp
    )
    target_link_libraries(${TEST_SUSPEND_RESUME_CLIENT}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        ${DLT_LIBRARIES}
        vsomeip_utilities
    )

    set(TEST_SUSPEND_RESUME_SERVICE_CONFIG_FILE ${TEST_SUSPEND_RESUME_SERVICE}.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/suspend_resume_tests/conf/${TEST_SUSPEND_RESUME_SERVICE_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/suspend_resume_tests/${TEST_SUSPEND_RESUME_SERVICE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/suspend_resume_tests/${TEST_SUSPEND_RESUME_SERVICE_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUSPEND_RESUME_SERVICE_CONFIG_FILE}
        ${TEST_SUSPEND_RESUME_CLIENT}
    )

    set(TEST_SUSPEND_RESUME_CLIENT_CONFIG_FILE ${TEST_SUSPEND_RESUME_CLIENT}.json)
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/suspend_resume_tests/conf/${TEST_SUSPEND_RESUME_CLIENT_CONFIG_FILE}.in
        ${NETWORK_TEST_SRC_DIR}/suspend_resume_tests/${TEST_SUSPEND_RESUME_CLIENT_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/suspend_resume_tests/${TEST_SUSPEND_RESUME_CLIENT_CONFIG_FILE}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUSPEND_RESUME_CLIENT_CONFIG_FILE}
        ${TEST_SUSPEND_RESUME_CLIENT}
    )

    set(TEST_SUSPEND_RESUME_MASTER_START_SCRIPT ${TEST_SUSPEND_RESUME_NAME}_master_starter.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/suspend_resume_tests/${TEST_SUSPEND_RESUME_MASTER_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUSPEND_RESUME_MASTER_START_SCRIPT}
        ${TEST_SUSPEND_RESUME_CLIENT}
    )

    set(TEST_SUSPEND_RESUME_SLAVE_START_SCRIPT ${TEST_SUSPEND_RESUME_NAME}_slave_starter.sh)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/suspend_resume_tests/${TEST_SUSPEND_RESUME_SLAVE_START_SCRIPT}
        ${NETWORK_TEST_BIN_DIR}/${TEST_SUSPEND_RESUME_SLAVE_START_SCRIPT}
        ${TEST_SUSPEND_RESUME_CLIENT}
    )

endif()

##############################################################################
# Add for every test a dependency to gtest
##############################################################################

if(NOT ${TESTS_BAT})
    add_dependencies(${TEST_CONFIGURATION} gtest)
    add_dependencies(${TEST_APPLICATION} gtest)
    add_dependencies(${TEST_APPLICATION_SINGLE_PROCESS_NAME} gtest)
    add_dependencies(${TEST_APPLICATION_AVAILABILITY_NAME} gtest)
    add_dependencies(${TEST_MAGIC_COOKIES_CLIENT} gtest)
    add_dependencies(${TEST_MAGIC_COOKIES_SERVICE} gtest)
    add_dependencies(${TEST_HEADER_FACTORY} gtest)
    add_dependencies(${TEST_HEADER_FACTORY_CLIENT} gtest)
    add_dependencies(${TEST_HEADER_FACTORY_SERVICE} gtest)
    add_dependencies(${TEST_LOCAL_ROUTING_SERVICE} gtest)
    add_dependencies(${TEST_LOCAL_ROUTING_CLIENT} gtest)
    add_dependencies(${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE} gtest)
    add_dependencies(${TEST_PAYLOAD_SERVICE} gtest)
    add_dependencies(${TEST_PAYLOAD_CLIENT} gtest)
    add_dependencies(${TEST_BIG_PAYLOAD_SERVICE} gtest)
    add_dependencies(${TEST_BIG_PAYLOAD_CLIENT} gtest)
    add_dependencies(${TEST_CLIENT_ID_SERVICE} gtest)
    add_dependencies(${TEST_CLIENT_ID_UTILITY} gtest)
    add_dependencies(${TEST_DEBOUNCE_CLIENT} gtest)
    add_dependencies(${TEST_DEBOUNCE_SERVICE} gtest)
    add_dependencies(${TEST_DEBOUNCE_FILTER_CLIENT} gtest)
    add_dependencies(${TEST_DEBOUNCE_FILTER_SERVICE} gtest)
    add_dependencies(${TEST_DEBOUNCE_CALLBACK_CLIENT} gtest)
    add_dependencies(${TEST_DEBOUNCE_CALLBACK_SERVICE} gtest)
    add_dependencies(${TEST_DEBOUNCE_FREQUENCY_CLIENT} gtest)
    add_dependencies(${TEST_DEBOUNCE_FREQUENCY_SERVICE} gtest)
    add_dependencies(${TEST_SUBSCRIBE_NOTIFY_SERVICE} gtest)
    add_dependencies(${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SERVICE} gtest)
    add_dependencies(${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_CLIENT} gtest)
    add_dependencies(${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE} gtest)
    add_dependencies(${TEST_CPU_LOAD_SERVICE} gtest)
    add_dependencies(${TEST_CPU_LOAD_CLIENT} gtest)
    add_dependencies(${TEST_INITIAL_EVENT_SERVICE} gtest)
    add_dependencies(${TEST_INITIAL_EVENT_CLIENT} gtest)
    add_dependencies(${TEST_INITIAL_EVENT_AVAILABILITY_CHECKER} gtest)
    add_dependencies(${TEST_INITIAL_EVENT_STOP_SERVICE} gtest)
    add_dependencies(${TEST_OFFER_SERVICE} gtest)
    add_dependencies(${TEST_OFFER_CLIENT} gtest)
    add_dependencies(${TEST_OFFER_SERVICE_EXTERNAL} gtest)
    add_dependencies(${TEST_OFFER_EXTERNAL_SD_MESSAGE_SENDER} gtest)
    add_dependencies(${TEST_OFFERED_SERVICES_INFO_CLIENT} gtest)
    add_dependencies(${TEST_OFFERED_SERVICES_INFO_SERVICE} gtest)
    add_dependencies(${TEST_PENDING_SUBSCRIPTION_SERVICE} gtest)
    add_dependencies(${TEST_PENDING_SUBSCRIPTION_CLIENT} gtest)
    add_dependencies(${TEST_MALICIOUS_DATA_SERVICE} gtest)
    add_dependencies(${TEST_MALICIOUS_DATA_CLIENT} gtest)
    if (${TEST_SECURITY})
        add_dependencies(${TEST_SECURITY_SERVICE} gtest)
        add_dependencies(${TEST_SECURITY_CLIENT} gtest)
    endif()
    add_dependencies(${TEST_E2E_SERVICE} gtest)
    add_dependencies(${TEST_E2E_CLIENT} gtest)
    if (${TEST_E2E_PROFILE_04})
    add_dependencies(${TEST_E2E_PROFILE_04_SERVICE} gtest)
    add_dependencies(${TEST_E2E_PROFILE_04_CLIENT} gtest)
    endif()
    if (${TEST_E2E_PROFILE_07})
    add_dependencies(${TEST_E2E_PROFILE_07_SERVICE} gtest)
    add_dependencies(${TEST_E2E_PROFILE_07_CLIENT} gtest)
    endif()
    add_dependencies(${TEST_EVENT_SERVICE} gtest)
    add_dependencies(${TEST_EVENT_CLIENT} gtest)
    if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "QNX")
    add_dependencies(${TEST_NPDU_SERVICE_ONE} gtest)
    add_dependencies(${TEST_NPDU_SERVICE_TWO} gtest)
    add_dependencies(${TEST_NPDU_SERVICE_THREE} gtest)
    add_dependencies(${TEST_NPDU_SERVICE_FOUR} gtest)
    add_dependencies(${TEST_NPDU_CLIENT_ONE} gtest)
    add_dependencies(${TEST_NPDU_CLIENT_TWO} gtest)
    add_dependencies(${TEST_NPDU_CLIENT_THREE} gtest)
    add_dependencies(${TEST_NPDU_CLIENT_FOUR} gtest)
    add_dependencies(${TEST_NPDU_DAEMON_CLIENT} gtest)
    add_dependencies(${TEST_NPDU_DAEMON_SERVICE} gtest)
    endif()
    add_dependencies(${TEST_SOMEIPTP_CLIENT} gtest)
    add_dependencies(${TEST_SOMEIPTP_SERVICE} gtest)
    if(${TEST_SECOND_ADDRESS})
        add_dependencies(${TEST_SECOND_ADDRESS_CLIENT} gtest)
        add_dependencies(${TEST_SECOND_ADDRESS_SERVICE} gtest)
    endif()
    add_dependencies(${TEST_SUSPEND_RESUME_CLIENT} gtest)
    add_dependencies(${TEST_SUSPEND_RESUME_SERVICE} gtest)
else()
    add_dependencies(${TEST_APPLICATION} gtest)
    add_dependencies(${TEST_HEADER_FACTORY} gtest)
    add_dependencies(${TEST_HEADER_FACTORY_CLIENT} gtest)
    add_dependencies(${TEST_HEADER_FACTORY_SERVICE} gtest)
    add_dependencies(${TEST_PAYLOAD_SERVICE} gtest)
    add_dependencies(${TEST_PAYLOAD_CLIENT} gtest)
    add_dependencies(${TEST_BIG_PAYLOAD_SERVICE} gtest)
    add_dependencies(${TEST_BIG_PAYLOAD_CLIENT} gtest)
    add_dependencies(${TEST_LOCAL_ROUTING_SERVICE} gtest)
    add_dependencies(${TEST_LOCAL_ROUTING_CLIENT} gtest)
endif()

##############################################################################
# Add tests to the target build_network_tests
##############################################################################

if(NOT ${TESTS_BAT})
    add_dependencies(build_network_tests ${TEST_CONFIGURATION})
    add_dependencies(build_network_tests ${TEST_APPLICATION})
    add_dependencies(build_network_tests ${TEST_APPLICATION_SINGLE_PROCESS_NAME})
    add_dependencies(build_network_tests ${TEST_APPLICATION_AVAILABILITY_NAME})
    add_dependencies(build_network_tests ${TEST_MAGIC_COOKIES_CLIENT})
    add_dependencies(build_network_tests ${TEST_MAGIC_COOKIES_SERVICE})
    add_dependencies(build_network_tests ${TEST_HEADER_FACTORY})
    add_dependencies(build_network_tests ${TEST_HEADER_FACTORY_CLIENT})
    add_dependencies(build_network_tests ${TEST_HEADER_FACTORY_SERVICE})
    add_dependencies(build_network_tests ${TEST_LOCAL_ROUTING_SERVICE})
    add_dependencies(build_network_tests ${TEST_LOCAL_ROUTING_CLIENT})
    add_dependencies(build_network_tests ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE})
    add_dependencies(build_network_tests ${TEST_PAYLOAD_SERVICE})
    add_dependencies(build_network_tests ${TEST_PAYLOAD_CLIENT})
    add_dependencies(build_network_tests ${TEST_BIG_PAYLOAD_SERVICE})
    add_dependencies(build_network_tests ${TEST_BIG_PAYLOAD_CLIENT})
    add_dependencies(build_network_tests ${TEST_CLIENT_ID_SERVICE})
    add_dependencies(build_network_tests ${TEST_CLIENT_ID_UTILITY})
    add_dependencies(build_network_tests ${TEST_DEBOUNCE_CLIENT})
    add_dependencies(build_network_tests ${TEST_DEBOUNCE_SERVICE})
    add_dependencies(build_network_tests ${TEST_DEBOUNCE_FILTER_CLIENT})
    add_dependencies(build_network_tests ${TEST_DEBOUNCE_FILTER_SERVICE})
    add_dependencies(build_network_tests ${TEST_DEBOUNCE_CALLBACK_CLIENT})
    add_dependencies(build_network_tests ${TEST_DEBOUNCE_CALLBACK_SERVICE})
    add_dependencies(build_network_tests ${TEST_DEBOUNCE_FREQUENCY_CLIENT})
    add_dependencies(build_network_tests ${TEST_DEBOUNCE_FREQUENCY_SERVICE})
    add_dependencies(build_network_tests ${TEST_SUBSCRIBE_NOTIFY_SERVICE})
    add_dependencies(build_network_tests ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SERVICE})
    add_dependencies(build_network_tests ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_CLIENT})
    add_dependencies(build_network_tests ${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE})
    add_dependencies(build_network_tests ${TEST_CPU_LOAD_SERVICE})
    add_dependencies(build_network_tests ${TEST_CPU_LOAD_CLIENT})
    add_dependencies(build_network_tests ${TEST_INITIAL_EVENT_SERVICE})
    add_dependencies(build_network_tests ${TEST_INITIAL_EVENT_CLIENT})
    add_dependencies(build_network_tests ${TEST_INITIAL_EVENT_AVAILABILITY_CHECKER})
    add_dependencies(build_network_tests ${TEST_INITIAL_EVENT_STOP_SERVICE})
    add_dependencies(build_network_tests ${TEST_OFFER_SERVICE})
    add_dependencies(build_network_tests ${TEST_OFFER_CLIENT})
    add_dependencies(build_network_tests ${TEST_OFFER_SERVICE_EXTERNAL})
    add_dependencies(build_network_tests ${TEST_OFFER_EXTERNAL_SD_MESSAGE_SENDER})
    add_dependencies(build_network_tests ${TEST_OFFER_BIG_SERVICE})
    add_dependencies(build_network_tests ${TEST_OFFER_BIG_CLIENT})
    add_dependencies(build_network_tests ${TEST_OFFER_MULTIPLE_OFFERING})
    add_dependencies(build_network_tests ${TEST_RESTART_ROUTING_SERVICE})
    add_dependencies(build_network_tests ${TEST_RESTART_ROUTING_CLIENT})
    if (${TEST_SECURITY})
        add_dependencies(build_network_tests ${TEST_SECURITY_SERVICE})
        add_dependencies(build_network_tests ${TEST_SECURITY_CLIENT})
    endif()
    add_dependencies(build_network_tests ${TEST_OFFERED_SERVICES_INFO_CLIENT})
    add_dependencies(build_network_tests ${TEST_OFFERED_SERVICES_INFO_SERVICE})
    add_dependencies(build_network_tests ${TEST_PENDING_SUBSCRIPTION_SERVICE})
    add_dependencies(build_network_tests ${TEST_PENDING_SUBSCRIPTION_CLIENT})
    add_dependencies(build_network_tests ${TEST_MALICIOUS_DATA_SERVICE})
    add_dependencies(build_network_tests ${TEST_MALICIOUS_DATA_CLIENT})
    add_dependencies(build_network_tests ${TEST_E2E_SERVICE})
    add_dependencies(build_network_tests ${TEST_E2E_CLIENT})
    if (${TEST_E2E_PROFILE_04})
        add_dependencies(build_network_tests ${TEST_E2E_PROFILE_04_SERVICE})
        add_dependencies(build_network_tests ${TEST_E2E_PROFILE_04_CLIENT})
    endif()
    if (${TEST_E2E_PROFILE_07})
        add_dependencies(build_network_tests ${TEST_E2E_PROFILE_07_SERVICE})
        add_dependencies(build_network_tests ${TEST_E2E_PROFILE_07_CLIENT})
    endif()
    add_dependencies(build_network_tests ${TEST_EVENT_SERVICE})
    add_dependencies(build_network_tests ${TEST_EVENT_CLIENT})
    if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "QNX")
    add_dependencies(build_network_tests ${TEST_NPDU_SERVICE_ONE})
    add_dependencies(build_network_tests ${TEST_NPDU_SERVICE_TWO})
    add_dependencies(build_network_tests ${TEST_NPDU_SERVICE_THREE})
    add_dependencies(build_network_tests ${TEST_NPDU_SERVICE_FOUR})
    add_dependencies(build_network_tests ${TEST_NPDU_CLIENT_ONE})
    add_dependencies(build_network_tests ${TEST_NPDU_CLIENT_TWO})
    add_dependencies(build_network_tests ${TEST_NPDU_CLIENT_THREE})
    add_dependencies(build_network_tests ${TEST_NPDU_CLIENT_FOUR})
    add_dependencies(build_network_tests ${TEST_NPDU_DAEMON_CLIENT})
    add_dependencies(build_network_tests ${TEST_NPDU_DAEMON_SERVICE})
    endif()
    add_dependencies(build_network_tests ${TEST_SOMEIPTP_CLIENT})
    add_dependencies(build_network_tests ${TEST_SOMEIPTP_SERVICE})
    if(${TEST_SECOND_ADDRESS})
        add_dependencies(build_network_tests ${TEST_SECOND_ADDRESS_CLIENT})
        add_dependencies(build_network_tests ${TEST_SECOND_ADDRESS_SERVICE})
    endif()
    add_dependencies(build_network_tests ${TEST_SUSPEND_RESUME_CLIENT})
    add_dependencies(build_network_tests ${TEST_SUSPEND_RESUME_SERVICE})
else()
    add_dependencies(build_network_tests ${TEST_APPLICATION})
    add_dependencies(build_network_tests ${TEST_BIG_PAYLOAD_SERVICE})
    add_dependencies(build_network_tests ${TEST_BIG_PAYLOAD_CLIENT})
    add_dependencies(build_network_tests ${TEST_PAYLOAD_SERVICE})
    add_dependencies(build_network_tests ${TEST_PAYLOAD_CLIENT})
    add_dependencies(build_network_tests ${TEST_HEADER_FACTORY})
    add_dependencies(build_network_tests ${TEST_HEADER_FACTORY_CLIENT})
    add_dependencies(build_network_tests ${TEST_HEADER_FACTORY_SERVICE})
    add_dependencies(build_network_tests ${TEST_LOCAL_ROUTING_SERVICE})
    add_dependencies(build_network_tests ${TEST_LOCAL_ROUTING_CLIENT})
endif()

# some tests require the routingmanagerd
add_dependencies(build_network_tests routingmanagerd)

##############################################################################
# Add tests
##############################################################################

if(NOT ${TESTS_BAT})
    add_test(NAME ${TEST_CONFIGURATION}
        COMMAND ${TEST_CONFIGURATION}
    )

    # application test
    add_test(NAME ${TEST_APPLICATION}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_APPLICATION_STARTER}
    )
    set_tests_properties(${TEST_APPLICATION} PROPERTIES TIMEOUT 80)
    add_test(NAME ${TEST_APPLICATION_SINGLE_PROCESS_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_APPLICATION_SINGLE_PROCESS_STARTER}
    )
    set_tests_properties(${TEST_APPLICATION_SINGLE_PROCESS_NAME} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_APPLICATION_AVAILABILITY_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_APPLICATION_AVAILABILITY_STARTER}
    )
    set_tests_properties(${TEST_APPLICATION_AVAILABILITY_NAME} PROPERTIES TIMEOUT 120)

    # magic cookies test
    add_test(NAME ${TEST_MAGIC_COOKIES_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_MAGIC_COOKIES_STARTER}
    )
    set_tests_properties(${TEST_MAGIC_COOKIES_NAME} PROPERTIES TIMEOUT 250)

    # Header/Factory tets
    add_test(NAME ${TEST_HEADER_FACTORY_NAME} COMMAND ${TEST_HEADER_FACTORY})
    add_test(NAME ${TEST_HEADER_FACTORY_NAME}_send_receive
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_HEADER_FACTORY_STARTER}
    )

    # Routing tests
    add_test(NAME ${TEST_LOCAL_ROUTING_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_ROUTING_STARTER}
    )

    add_test(NAME ${TEST_EXTERNAL_LOCAL_ROUTING_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_LOCAL_ROUTING_STARTER}
    )
    set_tests_properties(${TEST_EXTERNAL_LOCAL_ROUTING_NAME} PROPERTIES TIMEOUT 120)

    # Payload tests
    add_test(NAME ${TEST_LOCAL_PAYLOAD_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_PAYLOAD_STARTER}
    )
    add_test(NAME ${TEST_LOCAL_PAYLOAD_HUGE_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_PAYLOAD_HUGE_STARTER}
    )
    set_tests_properties(${TEST_LOCAL_PAYLOAD_HUGE_NAME} PROPERTIES TIMEOUT 1800)
    add_test(NAME ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_STARTER}
    )
    add_test(NAME ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_STARTER}
    )
    set_tests_properties(${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_NAME} PROPERTIES TIMEOUT 480)
    add_test(NAME ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_STARTER}
    )
    set_tests_properties(${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_NAME} PROPERTIES TIMEOUT 480)

    # big payload tests
    add_test(NAME ${TEST_LOCAL_BIG_PAYLOAD_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_BIG_PAYLOAD_STARTER}
    )
    set_tests_properties(${TEST_LOCAL_BIG_PAYLOAD_NAME} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_LOCAL_BIG_PAYLOAD_NAME_RANDOM}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_BIG_PAYLOAD_STARTER} RANDOM
    )
    set_tests_properties(${TEST_LOCAL_BIG_PAYLOAD_NAME_RANDOM} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_LOCAL_BIG_PAYLOAD_NAME_LIMITED}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_BIG_PAYLOAD_STARTER} LIMITED
    )
    set_tests_properties(${TEST_LOCAL_BIG_PAYLOAD_NAME_LIMITED} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_LOCAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_BIG_PAYLOAD_STARTER} QUEUELIMITEDGENERAL
    )
    set_tests_properties(${TEST_LOCAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_LOCAL_TCP_BIG_PAYLOAD_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_TCP_BIG_PAYLOAD_STARTER}
    )
    set_tests_properties(${TEST_LOCAL_TCP_BIG_PAYLOAD_NAME} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_LOCAL_TCP_BIG_PAYLOAD_NAME_RANDOM}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_TCP_BIG_PAYLOAD_STARTER} RANDOM
    )
    set_tests_properties(${TEST_LOCAL_TCP_TCP_BIG_PAYLOAD_NAME_RANDOM} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_LOCAL_TCP_BIG_PAYLOAD_NAME_LIMITED}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_TCP_BIG_PAYLOAD_STARTER} LIMITED
    )
    set_tests_properties(${TEST_LOCAL_TCP_BIG_PAYLOAD_NAME_LIMITED} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_LOCAL_TCP_BIG_PAYLOAD_NAME_QUEUE_LIMITED}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_TCP_BIG_PAYLOAD_STARTER} QUEUELIMITEDGENERAL
    )
    set_tests_properties(${TEST_LOCAL_TCP_BIG_PAYLOAD_NAME_QUEUE_LIMITED} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_EXTERNAL_BIG_PAYLOAD_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER}
    )
    set_tests_properties(${TEST_EXTERNAL_BIG_PAYLOAD_NAME} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_EXTERNAL_BIG_PAYLOAD_NAME_RANDOM}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER} RANDOM
    )
    set_tests_properties(${TEST_EXTERNAL_BIG_PAYLOAD_NAME_RANDOM} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_EXTERNAL_BIG_PAYLOAD_NAME_LIMITED}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER} LIMITED
    )
    set_tests_properties(${TEST_EXTERNAL_BIG_PAYLOAD_NAME_LIMITED} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_EXTERNAL_BIG_PAYLOAD_NAME_LIMITED_GENERAL}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER} LIMITEDGENERAL
    )
    set_tests_properties(${TEST_EXTERNAL_BIG_PAYLOAD_NAME_LIMITED_GENERAL} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_EXTERNAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED_GENERAL}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER} QUEUELIMITEDGENERAL
    )
    set_tests_properties(${TEST_EXTERNAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED_GENERAL} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_EXTERNAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED_SPECIFIC}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER} QUEUELIMITEDSPECIFIC
    )
    set_tests_properties(${TEST_EXTERNAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED_SPECIFIC} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_EXTERNAL_BIG_PAYLOAD_NAME_UDP}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER} UDP
    )
    set_tests_properties(${TEST_EXTERNAL_BIG_PAYLOAD_NAME_UDP} PROPERTIES TIMEOUT 120)

    # client id tests
    add_test(NAME ${TEST_CLIENT_ID_NAME}_diff_client_ids_diff_ports
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_MASTER_STARTER} ${TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_CLIENT_ID_NAME}_diff_client_ids_diff_ports PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_CLIENT_ID_NAME}_diff_client_ids_same_ports
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_MASTER_STARTER} ${TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_CLIENT_ID_NAME}_diff_client_ids_same_ports PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_CLIENT_ID_NAME}_diff_client_ids_partial_same_ports
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_MASTER_STARTER} ${TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_CLIENT_ID_NAME}_diff_client_ids_partial_same_ports PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_CLIENT_ID_UTILITY}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_UTILITY})
    set_property(TEST ${TEST_CLIENT_ID_UTILITY}
        APPEND PROPERTY ENVIRONMENT
        "VSOMEIP_CONFIGURATION=${TEST_CLIENT_ID_UTILITY_CONFIG_FILE}")
    set_tests_properties(${TEST_CLIENT_ID_UTILITY} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_CLIENT_ID_UTILITY}_masked_511
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_UTILITY})
    set_property(TEST ${TEST_CLIENT_ID_UTILITY}_masked_511
        APPEND PROPERTY ENVIRONMENT
        "VSOMEIP_CONFIGURATION=${TEST_CLIENT_ID_UTILITY_MASKED_511_CONFIG_FILE}")
    set_tests_properties(${TEST_CLIENT_ID_UTILITY}_masked_511 PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_CLIENT_ID_UTILITY}_masked_4095
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_UTILITY})
    set_property(TEST ${TEST_CLIENT_ID_UTILITY}_masked_4095
        APPEND PROPERTY ENVIRONMENT
        "VSOMEIP_CONFIGURATION=${TEST_CLIENT_ID_UTILITY_MASKED_4095_CONFIG_FILE}")
    set_tests_properties(${TEST_CLIENT_ID_UTILITY}_masked_4095 PROPERTIES TIMEOUT 600)

    add_test(NAME ${TEST_CLIENT_ID_UTILITY}_masked_127
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_UTILITY})
    set_property(TEST ${TEST_CLIENT_ID_UTILITY}_masked_127
        APPEND PROPERTY ENVIRONMENT
        "VSOMEIP_CONFIGURATION=${TEST_CLIENT_ID_UTILITY_MASKED_127_CONFIG_FILE}")
    set_tests_properties(${TEST_CLIENT_ID_UTILITY}_masked_127 PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_CLIENT_ID_UTILITY}_discontinuous_masked_511
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_CLIENT_ID_UTILITY})
    set_property(TEST ${TEST_CLIENT_ID_UTILITY}_discontinuous_masked_511
        APPEND PROPERTY ENVIRONMENT
        "VSOMEIP_CONFIGURATION=${TEST_CLIENT_ID_UTILITY_MASKED_DISCONTINUOUS_511_CONFIG_FILE}")
    set_tests_properties(${TEST_CLIENT_ID_UTILITY}_discontinuous_masked_511 PROPERTIES TIMEOUT 120)

    # debounce tests
    add_test(NAME ${TEST_DEBOUNCE_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_DEBOUNCE_MASTER_STARTER})
    set_tests_properties(${TEST_DEBOUNCE_UTILITY} PROPERTIES TIMEOUT 30)

    # debounce filter tests
    add_test(NAME ${TEST_DEBOUNCE_FILTER_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_DEBOUNCE_FILTER_MASTER_STARTER})
    set_tests_properties(${TEST_DEBOUNCE_FILTER_UTILITY} PROPERTIES TIMEOUT 30)
    # debounce callback tests
    add_test(NAME ${TEST_DEBOUNCE_CALLBACK_NAME}
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_DEBOUNCE_CALLBACK_MASTER_STARTER})
    set_tests_properties(${TEST_DEBOUNCE_CALLBACK_NAME} PROPERTIES TIMEOUT 60)

    # debounce frequency tests
    add_test(NAME ${TEST_DEBOUNCE_FREQUENCY_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_DEBOUNCE_FREQUENCY_MASTER_STARTER})
    set_tests_properties(${TEST_DEBOUNCE_FREQUENCY_NAME} PROPERTIES TIMEOUT 30)

    # subscribe notify tests (local communication via UDS)
    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_udp_local_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_UDP_LOCAL_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_udp_local_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_tcp_local_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} TCP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_TCP_LOCAL_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_tcp_local_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_both_tcp_and_udp_local_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} TCP_AND_UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_both_tcp_and_udp_local_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_udp_local_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_UDP_LOCAL_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_udp_local_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_tcp_local_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} TCP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_TCP_LOCAL_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_tcp_local_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_both_tcp_and_udp_local_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} TCP_AND_UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_both_tcp_and_udp_local_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_partial_same_ports_both_tcp_and_udp_local_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} TCP_AND_UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_LOCAL_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_partial_same_ports_both_tcp_and_udp_local_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_same_service_id_udp_local_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_UDP_LOCAL_TCP_CONFIG_FILE} SAME_SERVICE_ID)
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_same_service_id_udp_local_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_autoconfig_udp_local_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_MASTER_LOCAL_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_same_service_id_udp_local_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_udp_local_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_STARTER} UDP ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_LOCAL_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_udp_local_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_tcp_local_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_STARTER} TCP ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_LOCAL_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_tcp_local_tcp PROPERTIES TIMEOUT 120)

    # subscribe_notify_tests (local communication via TCP)
    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} TCP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_both_tcp_and_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} TCP_AND_UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} TCP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_both_tcp_and_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} TCP_AND_UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_partial_same_ports_both_tcp_and_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} TCP_AND_UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_partial_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_same_service_id_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_UDP_CONFIG_FILE} SAME_SERVICE_ID)
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_same_service_id_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_autoconfig_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_same_service_id_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_STARTER} UDP ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_STARTER} TCP ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_tcp PROPERTIES TIMEOUT 120)

    # subscribe notify one id tests
    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER} UDP ${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER} TCP ${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_both_tcp_and_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER} TCP_AND_UDP ${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    # cpu load tests
    add_test(NAME ${TEST_CPU_LOAD_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_CPU_LOAD_MASTER_STARTER}
    )
    set_tests_properties(${TEST_CPU_LOAD_NAME} PROPERTIES TIMEOUT 3000)

    # initial event tests
    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE} UDP)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE} TCP)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_both_tcp_and_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE} TCP_AND_UDP)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE} UDP)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE} TCP)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_both_tcp_and_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE} TCP_AND_UDP)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_partial_same_ports_both_tcp_and_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE} TCP_AND_UDP)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_partial_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_same_service_id_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE} TCP_AND_UDP SAME_SERVICE_ID)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_same_service_id_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE} UDP MULTIPLE_EVENTS)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE} TCP MULTIPLE_EVENTS)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_both_tcp_and_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE} TCP_AND_UDP MULTIPLE_EVENTS)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE} UDP MULTIPLE_EVENTS)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE} TCP MULTIPLE_EVENTS)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_both_tcp_and_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE} TCP_AND_UDP MULTIPLE_EVENTS)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_partial_same_ports_both_tcp_and_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE} TCP_AND_UDP MULTIPLE_EVENTS)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_partial_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_same_service_id_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE} TCP_AND_UDP SAME_SERVICE_ID MULTIPLE_EVENTS)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_same_service_id_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE} UDP MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE} TCP MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_both_tcp_and_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE} TCP_AND_UDP MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE} UDP MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE} TCP MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_both_tcp_and_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE} TCP_AND_UDP MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_partial_same_ports_both_tcp_and_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE} TCP_AND_UDP MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_partial_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_same_service_id_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE} TCP_AND_UDP SAME_SERVICE_ID MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_same_service_id_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_partial_subscription_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_UDP_CONFIG_FILE} UDP MULTIPLE_EVENTS SUBSCRIBE_ONLY_ONE)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_partial_subscription_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_partial_subscription_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE} TCP MULTIPLE_EVENTS SUBSCRIBE_ONLY_ONE)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_partial_subscription_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_partial_subscription_both_tcp_and_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE} TCP_AND_UDP MULTIPLE_EVENTS SUBSCRIBE_ONLY_ONE)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_partial_subscription_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_udp_client_subscribes_twice
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_INITIAL_EVENT_MASTER_STARTER} ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_UDP_CONFIG_FILE} UDP CLIENT_SUBSCRIBES_TWICE)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_udp_client_subscribes_twice PROPERTIES TIMEOUT 120)

    # offer tests
    add_test(NAME ${TEST_OFFER_NAME}_local
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_OFFER_LOCAL_STARTER})
    set_tests_properties(${TEST_OFFER_NAME}_local PROPERTIES TIMEOUT 180)
    add_test(NAME ${TEST_OFFER_NAME}_external
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_OFFER_EXTERNAL_MASTER_STARTER})
    set_tests_properties(${TEST_OFFER_NAME}_local PROPERTIES TIMEOUT 360)

    add_test(NAME ${TEST_OFFER_BIG_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_OFFER_BIG_MASTER_STARTER})
    set_tests_properties(${TEST_OFFER_BIG_NAME} PROPERTIES TIMEOUT 360)

    # Multiple Offer tests
    add_test(NAME ${TEST_OFFER_MULTIPLE_OFFERING}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_OFFER_MULTIPLE_OFFERING_STARTER})
    set_tests_properties(${TEST_OFFER_MULTIPLE_OFFERING} PROPERTIES TIMEOUT 1800)

    # offered services info tets
    add_test(NAME ${TEST_OFFERED_SERVICES_INFO_NAME}_local
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_OFFERED_SERVICES_INFO_LOCAL_STARTER})
    set_tests_properties(${TEST_OFFERED_SERVICES_INFO_NAME}_local PROPERTIES TIMEOUT 180)

    # Restart-Routing tests
    add_test(NAME ${TEST_RESTART_ROUTING_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_RESTART_ROUTING_STARTER}
    )
    if (${TEST_SECURITY})
        # Security tests
        add_test(NAME ${TEST_SECURITY_NAME}
            COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SECURITY_SERVICE_LOCAL_START_SCRIPT}
        )
        add_test(NAME ${TEST_SECURITY_NAME}_external_allow
            COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SECURITY_EXTERNAL_MASTER_START_SCRIPT} security_test_config_client_external_allow.json --allow
        )
        add_test(NAME ${TEST_SECURITY_NAME}_external_deny
            COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SECURITY_EXTERNAL_MASTER_START_SCRIPT} security_test_config_client_external_deny.json --deny
        )
    endif()

    # pending subscriptions test
    add_test(NAME ${TEST_PENDING_SUBSCRIPTION_NAME}_subscribe
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER} SUBSCRIBE)
    set_tests_properties(${TEST_PENDING_SUBSCRIPTION_NAME}_subscribe PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_PENDING_SUBSCRIPTION_NAME}_alternating_subscribe_unsubscribe
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER} SUBSCRIBE_UNSUBSCRIBE)
    set_tests_properties(${TEST_PENDING_SUBSCRIPTION_NAME}_alternating_subscribe_unsubscribe PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_PENDING_SUBSCRIPTION_NAME}_unsubscribe
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER} UNSUBSCRIBE)
    set_tests_properties(${TEST_PENDING_SUBSCRIPTION_NAME}_unsubscribe PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_PENDING_SUBSCRIPTION_NAME}_alternating_subscribe_unsubscribe_nack
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER} SUBSCRIBE_UNSUBSCRIBE_NACK)
    set_tests_properties(${TEST_PENDING_SUBSCRIPTION_NAME}_alternating_subscribe_unsubscribe_nack PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_PENDING_SUBSCRIPTION_NAME}_alternating_subscribe_unsubscribe_same_port
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER} SUBSCRIBE_UNSUBSCRIBE_SAME_PORT)
    set_tests_properties(${TEST_PENDING_SUBSCRIPTION_NAME}_alternating_subscribe_unsubscribe_same_port PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_PENDING_SUBSCRIPTION_NAME}_subscribe_resubscribe_mixed
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER} SUBSCRIBE_RESUBSCRIBE_MIXED)
    set_tests_properties(${TEST_PENDING_SUBSCRIPTION_NAME}_subscribe_resubscribe_mixed PROPERTIES TIMEOUT 300)

    add_test(NAME ${TEST_PENDING_SUBSCRIPTION_NAME}_subscribe_stopsubscribe_subscribe
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER} SUBSCRIBE_STOPSUBSCRIBE_SUBSCRIBE)
    set_tests_properties(${TEST_PENDING_SUBSCRIPTION_NAME}_subscribe_stopsubscribe_subscribe PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_PENDING_SUBSCRIPTION_NAME}_send_request_to_sd_port
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER} REQUEST_TO_SD)
    set_tests_properties(${TEST_PENDING_SUBSCRIPTION_NAME}_send_request_to_sd_port PROPERTIES TIMEOUT 180)

    # malicious data test
    add_test(NAME ${TEST_MALICIOUS_DATA_NAME}_events
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_MALICIOUS_DATA_MASTER_STARTER} MALICIOUS_EVENTS)
    set_tests_properties(${TEST_MALICIOUS_DATA_NAME}_events PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_MALICIOUS_DATA_NAME}_protocol_version
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_MALICIOUS_DATA_MASTER_STARTER} PROTOCOL_VERSION)
    set_tests_properties(${TEST_MALICIOUS_DATA_NAME}_protocol_version PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_MALICIOUS_DATA_NAME}_message_type
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_MALICIOUS_DATA_MASTER_STARTER} MESSAGE_TYPE)
    set_tests_properties(${TEST_MALICIOUS_DATA_NAME}_message_type PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_MALICIOUS_DATA_NAME}_return_code
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_MALICIOUS_DATA_MASTER_STARTER} RETURN_CODE)
    set_tests_properties(${TEST_MALICIOUS_DATA_NAME}_return_code PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_MALICIOUS_DATA_NAME}_wrong_header_fields_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_MALICIOUS_DATA_MASTER_STARTER} WRONG_HEADER_FIELDS_UDP)
    set_tests_properties(${TEST_MALICIOUS_DATA_NAME}_wrong_header_fields_udp PROPERTIES TIMEOUT 180)

    # npdu tests
    add_test(NAME ${TEST_NPDU_NAME}_udp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_NPDU_STARTER} UDP sync)
    set_tests_properties(${TEST_NPDU_NAME}_udp PROPERTIES TIMEOUT 840)

    add_test(NAME ${TEST_NPDU_NAME}_tcp
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_NPDU_STARTER} TCP sync)
    set_tests_properties(${TEST_NPDU_NAME}_tcp PROPERTIES TIMEOUT 840)

    # e2e tests
    add_test(NAME ${TEST_E2E_NAME}_external
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_E2E_EXTERNAL_MASTER_START_SCRIPT} e2e_test_client_external.json)
    set_tests_properties(${TEST_E2E_NAME}_external PROPERTIES TIMEOUT 180)

    if (${TEST_E2E_PROFILE_04})
    add_test(NAME ${TEST_E2E_PROFILE_04_NAME}_external
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_E2E_PROFILE_04_EXTERNAL_MASTER_START_SCRIPT} e2e_profile_04_test_client_external.json)
    set_tests_properties(${TEST_E2E_PROFILE_04_NAME}_external PROPERTIES TIMEOUT 180)
    endif ()

    if (${TEST_E2E_PROFILE_07})
    add_test(NAME ${TEST_E2E_PROFILE_07_NAME}_external
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_E2E_PROFILE_07_EXTERNAL_MASTER_START_SCRIPT} e2e_profile_07_test_client_external.json)
    set_tests_properties(${TEST_E2E_PROFILE_07_NAME}_external PROPERTIES TIMEOUT 180)
    endif ()

    # event tests
    add_test(NAME ${TEST_EVENT_NAME}_payload_fixed_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EVENT_MASTER_START_SCRIPT} PAYLOAD_FIXED UDP)
    set_tests_properties(${TEST_EVENT_NAME}_payload_fixed_udp PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_EVENT_NAME}_payload_fixed_tcp
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EVENT_MASTER_START_SCRIPT} PAYLOAD_FIXED TCP)
    set_tests_properties(${TEST_EVENT_NAME}_payload_fixed_tcp PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_EVENT_NAME}_payload_dynamic_udp
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EVENT_MASTER_START_SCRIPT} PAYLOAD_DYNAMIC UDP)
    set_tests_properties(${TEST_EVENT_NAME}_payload_dynamic_udp PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_EVENT_NAME}_payload_dynamic_tcp
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EVENT_MASTER_START_SCRIPT} PAYLOAD_DYNAMIC TCP)
    set_tests_properties(${TEST_EVENT_NAME}_payload_dynamic_tcp PROPERTIES TIMEOUT 180)

    # SOME/IP-TP tests
    add_test(NAME ${TEST_SOMEIPTP_NAME}_in_sequence
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SOMEIPTP_MASTER_START_SCRIPT} IN_SEQUENCE)
    set_tests_properties(${TEST_SOMEIPTP_NAME}_in_sequence PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_SOMEIPTP_NAME}_mixed
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SOMEIPTP_MASTER_START_SCRIPT} MIXED)
    set_tests_properties(${TEST_SOMEIPTP_NAME}_mixed PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_SOMEIPTP_NAME}_incomplete
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SOMEIPTP_MASTER_START_SCRIPT} INCOMPLETE)
    set_tests_properties(${TEST_SOMEIPTP_NAME}_incomplete PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_SOMEIPTP_NAME}_duplicate
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SOMEIPTP_MASTER_START_SCRIPT} DUPLICATE)
    set_tests_properties(${TEST_SOMEIPTP_NAME}_duplicate PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_SOMEIPTP_NAME}_overlap
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SOMEIPTP_MASTER_START_SCRIPT} OVERLAP)
    set_tests_properties(${TEST_SOMEIPTP_NAME}_overlap PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_SOMEIPTP_NAME}_overlap_front_back
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SOMEIPTP_MASTER_START_SCRIPT} OVERLAP_FRONT_BACK)
    set_tests_properties(${TEST_SOMEIPTP_NAME}_overlap_front_back PROPERTIES TIMEOUT 180)

    if(${TEST_SECOND_ADDRESS})
        add_test(NAME ${TEST_SECOND_ADDRESS_NAME}_second_ip_address_service_udp
            COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SECOND_ADDRESS_MASTER_START_SCRIPT} SERVICE UDP
        )
        set_tests_properties(${TEST_SECOND_ADDRESS_NAME}_second_ip_address_service_udp PROPERTIES TIMEOUT 180)

        add_test(NAME ${TEST_SECOND_ADDRESS_NAME}_second_ip_address_client_udp
            COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SECOND_ADDRESS_MASTER_START_SCRIPT} CLIENT UDP
        )
        set_tests_properties(${TEST_SECOND_ADDRESS_NAME}_second_ip_address_client_udp PROPERTIES TIMEOUT 180)
    endif()

    # suspend resume test
    add_test(NAME ${TEST_SUSPEND_RESUME_NAME}_initial
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_SUSPEND_RESUME_MASTER_START_SCRIPT} SERVICE
    )

else()
    # Routing tests
    add_test(NAME ${TEST_LOCAL_ROUTING_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_ROUTING_STARTER}
    )

    # application test
    add_test(NAME ${TEST_APPLICATION}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_APPLICATION_STARTER}
    )
    set_tests_properties(${TEST_APPLICATION} PROPERTIES TIMEOUT 80)

    # big payload tests
    add_test(NAME ${TEST_LOCAL_BIG_PAYLOAD_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_BIG_PAYLOAD_STARTER}
    )
    set_tests_properties(${TEST_LOCAL_BIG_PAYLOAD_NAME} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_LOCAL_BIG_PAYLOAD_NAME_RANDOM}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_BIG_PAYLOAD_STARTER} RANDOM
    )
    set_tests_properties(${TEST_LOCAL_BIG_PAYLOAD_NAME_RANDOM} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_LOCAL_BIG_PAYLOAD_NAME_LIMITED}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_BIG_PAYLOAD_STARTER} LIMITED
    )
    set_tests_properties(${TEST_LOCAL_BIG_PAYLOAD_NAME_LIMITED} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_LOCAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_BIG_PAYLOAD_STARTER} QUEUELIMITEDGENERAL
    )
    set_tests_properties(${TEST_LOCAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_LOCAL_TCP_BIG_PAYLOAD_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_TCP_BIG_PAYLOAD_STARTER}
    )
    set_tests_properties(${TEST_LOCAL_TCP_BIG_PAYLOAD_NAME} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_LOCAL_TCP_BIG_PAYLOAD_NAME_RANDOM}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_TCP_BIG_PAYLOAD_STARTER} RANDOM
    )
    set_tests_properties(${TEST_LOCAL_TCP_TCP_BIG_PAYLOAD_NAME_RANDOM} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_LOCAL_TCP_BIG_PAYLOAD_NAME_LIMITED}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_TCP_BIG_PAYLOAD_STARTER} LIMITED
    )
    set_tests_properties(${TEST_LOCAL_TCP_BIG_PAYLOAD_NAME_LIMITED} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_LOCAL_TCP_BIG_PAYLOAD_NAME_QUEUE_LIMITED}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_TCP_BIG_PAYLOAD_STARTER} QUEUELIMITEDGENERAL
    )
    set_tests_properties(${TEST_LOCAL_TCP_BIG_PAYLOAD_NAME_QUEUE_LIMITED} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_EXTERNAL_BIG_PAYLOAD_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER}
    )
    set_tests_properties(${TEST_EXTERNAL_BIG_PAYLOAD_NAME} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_EXTERNAL_BIG_PAYLOAD_NAME_RANDOM}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER} RANDOM
    )
    set_tests_properties(${TEST_EXTERNAL_BIG_PAYLOAD_NAME_RANDOM} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_EXTERNAL_BIG_PAYLOAD_NAME_LIMITED}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER} LIMITED
    )
    set_tests_properties(${TEST_EXTERNAL_BIG_PAYLOAD_NAME_LIMITED} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_EXTERNAL_BIG_PAYLOAD_NAME_LIMITED_GENERAL}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER} LIMITEDGENERAL
    )
    set_tests_properties(${TEST_EXTERNAL_BIG_PAYLOAD_NAME_LIMITED_GENERAL} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_EXTERNAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED_GENERAL}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER} QUEUELIMITEDGENERAL
    )
    set_tests_properties(${TEST_EXTERNAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED_GENERAL} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_EXTERNAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED_SPECIFIC}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER} QUEUELIMITEDSPECIFIC
    )
    set_tests_properties(${TEST_EXTERNAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED_SPECIFIC} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_EXTERNAL_BIG_PAYLOAD_NAME_UDP}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER} UDP
    )
    set_tests_properties(${TEST_EXTERNAL_BIG_PAYLOAD_NAME_UDP} PROPERTIES TIMEOUT 120)

    # Header/Factory tests
    add_test(NAME ${TEST_HEADER_FACTORY_NAME} COMMAND ${TEST_HEADER_FACTORY})
    add_test(NAME ${TEST_HEADER_FACTORY_NAME}_send_receive
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_HEADER_FACTORY_STARTER}
    )

    # Payload tests
    add_test(NAME ${TEST_LOCAL_PAYLOAD_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_PAYLOAD_STARTER}
    )
    add_test(NAME ${TEST_LOCAL_PAYLOAD_HUGE_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_LOCAL_PAYLOAD_HUGE_STARTER}
    )
    set_tests_properties(${TEST_LOCAL_PAYLOAD_HUGE_NAME} PROPERTIES TIMEOUT 1800)

    add_test(NAME ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_NAME}
        COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_STARTER}
    )
endif()
