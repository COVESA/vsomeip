# Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

##############################################################################
# google test

# check for set environment variable
if(DEFINED ENV{GTEST_ROOT})
    message("GTEST_ROOT is set. GTEST_ROOT = $ENV{GTEST_ROOT}")
else()
    message("GTEST_ROOT is not defined. For building the tests environment variable
        GTEST_ROOT have to be defined. Tests can not be built.")
    # early exit
    return()  # test can not be build -> make commands build_tests and check are not available
endif()

# build google test as static library (always) -> therefore deactivate BUILD_SHARED_LIBS in case it is active
set(BUILD_SHARED_LIBS_AUTOMATIC_OFF 0)
if("${BUILD_SHARED_LIBS}" STREQUAL "ON")
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_SHARED_LIBS_AUTOMATIC_OFF 1)
endif()
add_subdirectory($ENV{GTEST_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/gtest EXCLUDE_FROM_ALL)
if("${BUILD_SHARED_LIBS_AUTOMATIC_OFF}" STREQUAL "1")
    set(BUILD_SHARED_LIBS ON)
    set(BUILD_SHARED_LIBS_AUTOMATIC_OFF 0)
endif()

##############################################################################
# build tests

enable_testing()
set(TESTS_BAT "OFF" CACHE BOOL
    "Controls whether only BAT tests should be build or not")
set(TEST_SYMLINK_CONFIG_FILES "OFF" CACHE BOOL
    "Controls if the json and scripts needed needed to run
    the tests are copied or symlinked into the build directroy (ignored on Windows)")
set(TEST_SYMLINK_CONFIG_FILES_RELATIVE "OFF" CACHE BOOL
    "Controls if the json and scripts needed needed to run the tests are symlinked
    relatively into the build directroy (ignored on Windows)")

set(TEST_IP_DEFAULT_VALUE "XXX.XXX.XXX.XXX")
set(TEST_IP_MASTER "${TEST_IP_DEFAULT_VALUE}" CACHE STRING
    "The IP address of the interface which will act as test master")
set(TEST_IP_SLAVE "${TEST_IP_DEFAULT_VALUE}" CACHE STRING
    "The IP address of the interface which will act as test slave")

if((${TEST_IP_MASTER} STREQUAL ${TEST_IP_DEFAULT_VALUE}) OR
    (${TEST_IP_SLAVE} STREQUAL ${TEST_IP_DEFAULT_VALUE}))
    message(WARNING "TEST_IP_MASTER and/or TEST_IP_SLAVE isn't set. "
                    "Only local tests will be runnable "
                    "Please specify them via for example "
                    "-DTEST_IP_MASTER=10.0.3.1 -DTEST_IP_SLAVE=10.0.3.125")
endif()

add_custom_target(build_tests)
add_dependencies(build_tests vsomeip)
add_dependencies(build_tests vsomeip-sd)

set(CMAKE_CTEST_COMMAND ctest -V)
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})

add_dependencies(check build_tests)

##############################################################################
# add test directory

# Add the gtest header files to the include files
include_directories(
    ${gtest_SOURCE_DIR}/include
)

set(TEST_LINK_LIBRARIES gtest)

# Function to copy files into the build directory (or anywhere else)
# On unixoid systems this function will create symlinks instead
# SOURCE_PATH: Path to the file which should be copied
# DESTINATION_PATH: destination file
# TARGET_TO_DEPEND: The copying of the file will be added as
#                   a dependency to this target
function(copy_to_builddir SOURCE_PATH DESTINATION_PATH TARGET_TO_DEPEND)
    if(${CMAKE_SYSTEM_NAME} MATCHES "Windows" OR NOT ${TEST_SYMLINK_CONFIG_FILES})
        add_custom_command(
            OUTPUT "${DESTINATION_PATH}"
            COMMAND ${CMAKE_COMMAND} -E copy "${SOURCE_PATH}" "${DESTINATION_PATH}"
            DEPENDS "${SOURCE_PATH}"
            COMMENT "Copying \"${SOURCE_PATH}\" into build directory"
        )
    else()
        if(${TEST_SYMLINK_CONFIG_FILES_RELATIVE})
            add_custom_command(
                OUTPUT "${DESTINATION_PATH}"
                # Create a relative link
                COMMAND ln -s -r "${SOURCE_PATH}" "${DESTINATION_PATH}"
                DEPENDS "${SOURCE_PATH}"
                COMMENT "Symlinking \"${SOURCE_PATH}\" into build directory"
            )
        else()
            add_custom_command(
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
    get_filename_component(FILENAME ${SOURCE_PATH} NAME)
    string(RANDOM LENGTH 4 ALPHABET 0123456789 RANDOMNUMBER)
    if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        add_custom_target(copy_${FILENAME}_${RANDOMNUMBER}
            DEPENDS "${DESTINATION_PATH}"
        )
        add_dependencies(${TARGET_TO_DEPEND} copy_${FILENAME}_${RANDOMNUMBER})
    else()
        add_custom_target(symlink_${FILENAME}_${RANDOMNUMBER}
            DEPENDS "${DESTINATION_PATH}"
        )
        add_dependencies(${TARGET_TO_DEPEND} symlink_${FILENAME}_${RANDOMNUMBER})
    endif()
endfunction()

##############################################################################
# configuration-test
##############################################################################
if(NOT ${TESTS_BAT})    
    set(TEST_CONFIGURATION configuration-test)

    add_executable(${TEST_CONFIGURATION} configuration_tests/configuration-test.cpp)
    target_link_libraries(${TEST_CONFIGURATION}
        vsomeip
        vsomeip-cfg
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )
 
    # The following will make sure that ${TEST_CONFIGURATION_CONFIG_FILE} is copied
    # from the config folder into the test folder in the builddirectory
    # This makes it possible to call the configuration test within the build directory
    set(TEST_CONFIGURATION_CONFIG_FILE configuration-test.json)
    set(TEST_CONFIGURATION_DEPRECATED_CONFIG_FILE configuration-test-deprecated.json)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/configuration_tests/${TEST_CONFIGURATION_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_CONFIGURATION_CONFIG_FILE}
        ${TEST_CONFIGURATION}
    )
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/configuration_tests/${TEST_CONFIGURATION_DEPRECATED_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_CONFIGURATION_DEPRECATED_CONFIG_FILE}
        ${TEST_CONFIGURATION}
    )
endif()
##############################################################################
# application test
##############################################################################
if(NOT ${TESTS_BAT})
    set(TEST_APPLICATION application_test)

    add_executable(${TEST_APPLICATION} application_tests/${TEST_APPLICATION}.cpp)
    target_link_libraries(${TEST_APPLICATION}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    set(TEST_APPLICATION_SINGLE_PROCESS_NAME ${TEST_APPLICATION}_single_process)
    add_executable(${TEST_APPLICATION_SINGLE_PROCESS_NAME} application_tests/${TEST_APPLICATION_SINGLE_PROCESS_NAME}.cpp)
    target_link_libraries(${TEST_APPLICATION_SINGLE_PROCESS_NAME}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    set(TEST_APPLICATION_AVAILABILITY_NAME ${TEST_APPLICATION}_availability)
    add_executable(${TEST_APPLICATION_AVAILABILITY_NAME} application_tests/${TEST_APPLICATION_AVAILABILITY_NAME}.cpp)
    target_link_libraries(${TEST_APPLICATION_AVAILABILITY_NAME}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    set(TEST_APPLICATION_SINGLE_PROCESS_CONFIGURATION_FILE ${TEST_APPLICATION}_single_process.json)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/application_tests/${TEST_APPLICATION_SINGLE_PROCESS_CONFIGURATION_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_APPLICATION_SINGLE_PROCESS_CONFIGURATION_FILE}
        ${TEST_APPLICATION}_single_process
    )

    set(TEST_APPLICATION_CONFIGURATION_FILE ${TEST_APPLICATION}.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/application_tests/conf/${TEST_APPLICATION_CONFIGURATION_FILE}.in
        ${PROJECT_BINARY_DIR}/test/application_tests/${TEST_APPLICATION_CONFIGURATION_FILE}
        @ONLY)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/application_tests/${TEST_APPLICATION_CONFIGURATION_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_APPLICATION_CONFIGURATION_FILE}
        ${TEST_APPLICATION}
    )

    set(TEST_APPLICATION_CONFIGURATION_FILE_DAEMON ${TEST_APPLICATION}_daemon.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/application_tests/conf/${TEST_APPLICATION_CONFIGURATION_FILE_DAEMON}.in
        ${PROJECT_BINARY_DIR}/test/application_tests/${TEST_APPLICATION_CONFIGURATION_FILE_DAEMON}
        @ONLY)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/application_tests/${TEST_APPLICATION_CONFIGURATION_FILE_DAEMON}
        ${PROJECT_BINARY_DIR}/test/${TEST_APPLICATION_CONFIGURATION_FILE_DAEMON}
        ${TEST_APPLICATION}
    )

    set(TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE ${TEST_APPLICATION}_no_dispatch_threads.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/application_tests/conf/${TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE}.in
        ${PROJECT_BINARY_DIR}/test/application_tests/${TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE}
        @ONLY)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/application_tests/${TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE}
        ${TEST_APPLICATION}
    )

    set(TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE_DAEMON ${TEST_APPLICATION}_no_dispatch_threads_daemon.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/application_tests/conf/${TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE_DAEMON}.in
        ${PROJECT_BINARY_DIR}/test/application_tests/${TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE_DAEMON}
        @ONLY)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/application_tests/${TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE_DAEMON}
        ${PROJECT_BINARY_DIR}/test/${TEST_APPLICATION_NO_DISPATCH_CONFIGURATION_FILE_DAEMON}
        ${TEST_APPLICATION}
        )

    set(TEST_APPLICATION_STARTER ${TEST_APPLICATION}_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/application_tests/${TEST_APPLICATION_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_APPLICATION_STARTER}
        ${TEST_APPLICATION}
    )
 
    set(TEST_APPLICATION_SINGLE_PROCESS_STARTER ${TEST_APPLICATION}_single_process_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/application_tests/${TEST_APPLICATION_SINGLE_PROCESS_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_APPLICATION_SINGLE_PROCESS_STARTER}
        ${TEST_APPLICATION}_single_process
    )

    set(TEST_APPLICATION_AVAILABILITY_STARTER ${TEST_APPLICATION_AVAILABILITY_NAME}_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/application_tests/${TEST_APPLICATION_AVAILABILITY_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_APPLICATION_AVAILABILITY_STARTER}
        ${TEST_APPLICATION_SINGLE_PROCESS_NAME}
    )
endif()
##############################################################################
# magic-cookies-test-client
##############################################################################
if(NOT ${TESTS_BAT})
    set(TEST_MAGIC_COOKIES_NAME magic_cookies_test)

    set(TEST_MAGIC_COOKIES_CLIENT ${TEST_MAGIC_COOKIES_NAME}_client)
    add_executable(${TEST_MAGIC_COOKIES_CLIENT} magic_cookies_tests/${TEST_MAGIC_COOKIES_CLIENT}.cpp)
    target_link_libraries(${TEST_MAGIC_COOKIES_CLIENT}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${CMAKE_THREAD_LIBS_INIT}
        ${TEST_LINK_LIBRARIES}
    )

    set(TEST_MAGIC_COOKIES_SERVICE ${TEST_MAGIC_COOKIES_NAME}_service)
    add_executable(${TEST_MAGIC_COOKIES_SERVICE} magic_cookies_tests/${TEST_MAGIC_COOKIES_SERVICE}.cpp)
    target_link_libraries(${TEST_MAGIC_COOKIES_SERVICE}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${CMAKE_THREAD_LIBS_INIT}
        ${TEST_LINK_LIBRARIES}
    )

    # Copy config file for client into $BUILDDIR/test
    set(TEST_MAGIC_COOKIES_CLIENT_CONFIG_FILE ${TEST_MAGIC_COOKIES_CLIENT}.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/magic_cookies_tests/conf/${TEST_MAGIC_COOKIES_CLIENT_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/magic_cookies_tests/${TEST_MAGIC_COOKIES_CLIENT_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/magic_cookies_tests/${TEST_MAGIC_COOKIES_CLIENT_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_MAGIC_COOKIES_CLIENT_CONFIG_FILE}
        ${TEST_MAGIC_COOKIES_CLIENT}
    )

    # Copy bashscript to start client into $BUILDDIR/test
    set(TEST_MAGIC_COOKIES_CLIENT_START_SCRIPT ${TEST_MAGIC_COOKIES_CLIENT}_start.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/magic_cookies_tests/${TEST_MAGIC_COOKIES_CLIENT_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_MAGIC_COOKIES_CLIENT_START_SCRIPT}
        ${TEST_MAGIC_COOKIES_CLIENT}
    )

    set(TEST_MAGIC_COOKIES_SERVICE magic_cookies_test_service)
    # Copy config file for service into $BUILDDIR/test
    set(TEST_MAGIC_COOKIES_SERVICE_CONFIG_FILE ${TEST_MAGIC_COOKIES_SERVICE}.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/magic_cookies_tests/conf/${TEST_MAGIC_COOKIES_SERVICE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/magic_cookies_tests/${TEST_MAGIC_COOKIES_SERVICE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/magic_cookies_tests/${TEST_MAGIC_COOKIES_SERVICE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_MAGIC_COOKIES_SERVICE_CONFIG_FILE}
        ${TEST_MAGIC_COOKIES_SERVICE}
    )

    # Copy bashscript to start service into $BUILDDIR/test
    set(TEST_MAGIC_COOKIES_SERVICE_START_SCRIPT ${TEST_MAGIC_COOKIES_SERVICE}_start.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/magic_cookies_tests/${TEST_MAGIC_COOKIES_SERVICE_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_MAGIC_COOKIES_SERVICE_START_SCRIPT}
        ${TEST_MAGIC_COOKIES_CLIENT}
    )

    # Copy bashscript to start client and server $BUILDDIR/test
    set(TEST_MAGIC_COOKIES_STARTER ${TEST_MAGIC_COOKIES_NAME}_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/magic_cookies_tests/${TEST_MAGIC_COOKIES_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_MAGIC_COOKIES_STARTER}
        ${TEST_MAGIC_COOKIES_CLIENT}
    )
endif()
##############################################################################
# someip-header-factory-test
##############################################################################
if(NOT ${TESTS_BAT})
    set(TEST_HEADER_FACTORY_NAME header_factory_test)

    set(TEST_HEADER_FACTORY header_factory_test)
    add_executable(${TEST_HEADER_FACTORY} header_factory_tests/header_factory_test.cpp)
    target_link_libraries(${TEST_HEADER_FACTORY}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${CMAKE_THREAD_LIBS_INIT}
        ${TEST_LINK_LIBRARIES}
    )

    ##############################################################################
    # Now comes the second part of the header factory test which consists of ouf
    # a client and a service both with settings file and bash scripts to start them
    set(TEST_HEADER_FACTORY_CLIENT header_factory_test_client)
    add_executable(${TEST_HEADER_FACTORY_CLIENT} header_factory_tests/${TEST_HEADER_FACTORY_CLIENT}.cpp)
    target_link_libraries(${TEST_HEADER_FACTORY_CLIENT}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${CMAKE_THREAD_LIBS_INIT}
        ${TEST_LINK_LIBRARIES}
    )

    # Copy config file for client into $BUILDDIR/test
    set(TEST_HEADER_FACTORY_CLIENT_CONFIG_FILE ${TEST_HEADER_FACTORY_CLIENT}.json)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/header_factory_tests/${TEST_HEADER_FACTORY_CLIENT_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_HEADER_FACTORY_CLIENT_CONFIG_FILE}
        ${TEST_HEADER_FACTORY_CLIENT}
    )

    # Copy bashscript to start client into $BUILDDIR/test
    set(TEST_HEADER_FACTORY_CLIENT_START_SCRIPT ${TEST_HEADER_FACTORY_CLIENT}_start.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/header_factory_tests/${TEST_HEADER_FACTORY_CLIENT_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_HEADER_FACTORY_CLIENT_START_SCRIPT}
        ${TEST_HEADER_FACTORY_CLIENT}
    )

    set(TEST_HEADER_FACTORY_SERVICE header_factory_test_service)
    add_executable(${TEST_HEADER_FACTORY_SERVICE} header_factory_tests/${TEST_HEADER_FACTORY_SERVICE}.cpp)
    target_link_libraries(${TEST_HEADER_FACTORY_SERVICE}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${CMAKE_THREAD_LIBS_INIT}
        ${TEST_LINK_LIBRARIES}
    )

    # Copy config file for service into $BUILDDIR/test
    set(TEST_HEADER_FACTORY_SERVICE_CONFIG_FILE ${TEST_HEADER_FACTORY_SERVICE}.json)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/header_factory_tests/${TEST_HEADER_FACTORY_SERVICE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_HEADER_FACTORY_SERVICE_CONFIG_FILE}
        ${TEST_HEADER_FACTORY_SERVICE}
    )

    # Copy bashscript to start service into $BUILDDIR/test
    set(TEST_HEADER_FACTORY_SERVICE_START_SCRIPT ${TEST_HEADER_FACTORY_SERVICE}_start.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/header_factory_tests/${TEST_HEADER_FACTORY_SERVICE_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_HEADER_FACTORY_SERVICE_START_SCRIPT}
        ${TEST_HEADER_FACTORY_SERVICE}
    )

    # Copy bashscript to start client and server $BUILDDIR/test
    set(TEST_HEADER_FACTORY_STARTER header_factory_test_send_receive_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/header_factory_tests/${TEST_HEADER_FACTORY_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_HEADER_FACTORY_STARTER}
        ${TEST_HEADER_FACTORY_CLIENT}
    )
endif()
##############################################################################
# routing-test
##############################################################################

if(NOT ${TESTS_BAT})
    set(TEST_LOCAL_ROUTING_NAME local_routing_test)

    set(TEST_LOCAL_ROUTING_SERVICE local_routing_test_service)
    add_executable(${TEST_LOCAL_ROUTING_SERVICE} routing_tests/${TEST_LOCAL_ROUTING_SERVICE}.cpp)
    target_link_libraries(${TEST_LOCAL_ROUTING_SERVICE}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    # Copy config file for service into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_SERVICE_CONFIG_FILE ${TEST_LOCAL_ROUTING_SERVICE}.json)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/routing_tests/${TEST_LOCAL_ROUTING_SERVICE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_ROUTING_SERVICE_CONFIG_FILE}
        ${TEST_LOCAL_ROUTING_SERVICE}
    )

    # Copy bashscript to start service into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_SERVICE_START_SCRIPT ${TEST_LOCAL_ROUTING_SERVICE}_start.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/routing_tests/${TEST_LOCAL_ROUTING_SERVICE_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_ROUTING_SERVICE_START_SCRIPT}
        ${TEST_LOCAL_ROUTING_SERVICE}
    )

    set(TEST_LOCAL_ROUTING_CLIENT local_routing_test_client)
    add_executable(${TEST_LOCAL_ROUTING_CLIENT} routing_tests/${TEST_LOCAL_ROUTING_CLIENT}.cpp)
    target_link_libraries(${TEST_LOCAL_ROUTING_CLIENT}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    # Copy config file for client into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_CLIENT_CONFIG_FILE ${TEST_LOCAL_ROUTING_CLIENT}.json)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/routing_tests/${TEST_LOCAL_ROUTING_CLIENT_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_ROUTING_CLIENT_CONFIG_FILE}
        ${TEST_LOCAL_ROUTING_CLIENT}
    )

    # Copy bashscript to start client into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_CLIENT_START_SCRIPT ${TEST_LOCAL_ROUTING_CLIENT}_start.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/routing_tests/${TEST_LOCAL_ROUTING_CLIENT_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_ROUTING_CLIENT_START_SCRIPT}
        ${TEST_LOCAL_ROUTING_CLIENT}
    )

    # Copy bashscript to start client and server $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_STARTER local_routing_test_starter.sh)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/routing_tests/conf/${TEST_LOCAL_ROUTING_STARTER}.in
        ${PROJECT_BINARY_DIR}/test/routing_tests/${TEST_LOCAL_ROUTING_STARTER}
        @ONLY)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/routing_tests/${TEST_LOCAL_ROUTING_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_ROUTING_STARTER}
        ${TEST_LOCAL_ROUTING_CLIENT}
    )

    ##############################################################################
    set(TEST_EXTERNAL_LOCAL_ROUTING_NAME external_local_routing_test)
    set(TEST_EXTERNAL_LOCAL_ROUTING_SERVICE external_local_routing_test_service)
    add_executable(${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE} routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}.cpp)
    target_link_libraries(${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    # Copy config file for service into $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_CONFIG_FILE ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/routing_tests/conf/${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_CONFIG_FILE}
        ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}
    )

    # Copy bashscript to start service into $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_START_SCRIPT ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}_start.sh)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE_START_SCRIPT}
        ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}
    )

    # Copy bashscript to start external client into $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_ROUTING_CLIENT external_local_routing_test_client_external)
    set(TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_START_SCRIPT ${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT}_start.sh)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_START_SCRIPT}
        ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}
    )

    # Copy config file for client into $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_CONFIG_FILE ${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT}.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/routing_tests/conf/${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_ROUTING_CLIENT_CONFIG_FILE}
        ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}
    )

    # Copy bashscript to start client and server $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_ROUTING_STARTER external_local_routing_test_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/routing_tests/${TEST_EXTERNAL_LOCAL_ROUTING_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_ROUTING_STARTER}
        ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE}
    )
else()
    set(TEST_LOCAL_ROUTING_NAME local_routing_test)

    set(TEST_LOCAL_ROUTING_SERVICE local_routing_test_service)
    add_executable(${TEST_LOCAL_ROUTING_SERVICE} routing_tests/${TEST_LOCAL_ROUTING_SERVICE}.cpp)
    target_link_libraries(${TEST_LOCAL_ROUTING_SERVICE}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    # Copy config file for service into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_SERVICE_CONFIG_FILE ${TEST_LOCAL_ROUTING_SERVICE}.json)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/routing_tests/${TEST_LOCAL_ROUTING_SERVICE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_ROUTING_SERVICE_CONFIG_FILE}
        ${TEST_LOCAL_ROUTING_SERVICE}
    )

    # Copy bashscript to start service into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_SERVICE_START_SCRIPT ${TEST_LOCAL_ROUTING_SERVICE}_start.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/routing_tests/${TEST_LOCAL_ROUTING_SERVICE_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_ROUTING_SERVICE_START_SCRIPT}
        ${TEST_LOCAL_ROUTING_SERVICE}
    )

    set(TEST_LOCAL_ROUTING_CLIENT local_routing_test_client)
    add_executable(${TEST_LOCAL_ROUTING_CLIENT} routing_tests/${TEST_LOCAL_ROUTING_CLIENT}.cpp)
    target_link_libraries(${TEST_LOCAL_ROUTING_CLIENT}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    # Copy config file for client into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_CLIENT_CONFIG_FILE ${TEST_LOCAL_ROUTING_CLIENT}.json)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/routing_tests/${TEST_LOCAL_ROUTING_CLIENT_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_ROUTING_CLIENT_CONFIG_FILE}
        ${TEST_LOCAL_ROUTING_CLIENT}
    )

    # Copy bashscript to start client into $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_CLIENT_START_SCRIPT ${TEST_LOCAL_ROUTING_CLIENT}_start.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/routing_tests/${TEST_LOCAL_ROUTING_CLIENT_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_ROUTING_CLIENT_START_SCRIPT}
        ${TEST_LOCAL_ROUTING_CLIENT}
    )

    # Copy bashscript to start client and server $BUILDDIR/test
    set(TEST_LOCAL_ROUTING_STARTER local_routing_test_starter.sh)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/routing_tests/conf/${TEST_LOCAL_ROUTING_STARTER}.bat.in
        ${PROJECT_BINARY_DIR}/test/routing_tests/${TEST_LOCAL_ROUTING_STARTER}
        @ONLY)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/routing_tests/${TEST_LOCAL_ROUTING_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_ROUTING_STARTER}
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
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    # Copy config file for service into $BUILDDIR/test
    set(TEST_RESTART_ROUTING_SERVICE_CONFIG_FILE ${TEST_RESTART_ROUTING_SERVICE}.json)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/restart_routing_tests/${TEST_RESTART_ROUTING_SERVICE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_RESTART_ROUTING_SERVICE_CONFIG_FILE}
        ${TEST_RESTART_ROUTING_SERVICE}
    )

    # Copy bashscript to start service into $BUILDDIR/test
    set(TEST_RESTART_ROUTING_SERVICE_START_SCRIPT ${TEST_RESTART_ROUTING_SERVICE}_start.sh)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/restart_routing_tests/${TEST_RESTART_ROUTING_SERVICE_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_RESTART_ROUTING_SERVICE_START_SCRIPT}
        ${TEST_RESTART_ROUTING_SERVICE}
    )

    set(TEST_RESTART_ROUTING_CLIENT restart_routing_test_client)
    add_executable(${TEST_RESTART_ROUTING_CLIENT}
        restart_routing_tests/${TEST_RESTART_ROUTING_CLIENT}.cpp
    )
    target_link_libraries(${TEST_RESTART_ROUTING_CLIENT}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    # Copy config file for client into $BUILDDIR/test
    set(TEST_RESTART_ROUTING_CLIENT_CONFIG_FILE ${TEST_RESTART_ROUTING_CLIENT}.json)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/restart_routing_tests/${TEST_RESTART_ROUTING_CLIENT_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_RESTART_ROUTING_CLIENT_CONFIG_FILE}
        ${TEST_RESTART_ROUTING_CLIENT}
    )

    # Copy bashscript to start client into $BUILDDIR/test
    set(TEST_RESTART_ROUTING_CLIENT_START_SCRIPT ${TEST_RESTART_ROUTING_CLIENT}_start.sh)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/restart_routing_tests/${TEST_RESTART_ROUTING_CLIENT_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_RESTART_ROUTING_CLIENT_START_SCRIPT}
        ${TEST_RESTART_ROUTING_CLIENT}
    )

    # Copy bashscript to start client and server $BUILDDIR/test
    set(TEST_RESTART_ROUTING_STARTER restart_routing_test_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/restart_routing_tests/${TEST_RESTART_ROUTING_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_RESTART_ROUTING_STARTER}
        ${TEST_RESTART_ROUTING_CLIENT}
    )

    # Copy config file for autoconfig into $BUILDDIR/test
    set(TEST_RESTART_ROUTING_AUTO_CONFIG_FILE ${TEST_RESTART_ROUTING_NAME}_autoconfig.json)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/restart_routing_tests/${TEST_RESTART_ROUTING_AUTO_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_RESTART_ROUTING_AUTO_CONFIG_FILE}
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
        add_executable(${TEST_SECURITY_SERVICE} security_tests/${TEST_SECURITY_SERVICE}.cpp)
        target_link_libraries(${TEST_SECURITY_SERVICE}
            vsomeip
            ${Boost_LIBRARIES}
            ${DL_LIBRARY}
            ${TEST_LINK_LIBRARIES}
        )
    
        # Copy config file for service into $BUILDDIR/test
        set(TEST_SECURITY_SERVICE_CONFIG_FILE ${TEST_SECURITY_NAME}_config.json)
        configure_file(
            ${PROJECT_SOURCE_DIR}/test/security_tests/conf/${TEST_SECURITY_SERVICE_CONFIG_FILE}.in
            ${PROJECT_BINARY_DIR}/test/security_tests/${TEST_SECURITY_SERVICE_CONFIG_FILE}
            @ONLY)
        copy_to_builddir(
            ${PROJECT_SOURCE_DIR}/test/security_tests/${TEST_SECURITY_SERVICE_CONFIG_FILE}
            ${PROJECT_BINARY_DIR}/test/${TEST_SECURITY_SERVICE_CONFIG_FILE}
            ${TEST_SECURITY_SERVICE}
        )
    
        # Copy bashscript to start service into $BUILDDIR/test
        set(TEST_SECURITY_SERVICE_START_SCRIPT ${TEST_SECURITY_NAME}_start.sh)
        copy_to_builddir(
            ${PROJECT_SOURCE_DIR}/test/security_tests/${TEST_SECURITY_SERVICE_START_SCRIPT}
            ${PROJECT_BINARY_DIR}/test/${TEST_SECURITY_SERVICE_START_SCRIPT}
            ${TEST_SECURITY_SERVICE}
        )
    
        set(TEST_SECURITY_CLIENT security_test_client)
        add_executable(${TEST_SECURITY_CLIENT}
            security_tests/${TEST_SECURITY_CLIENT}.cpp
        )
        target_link_libraries(${TEST_SECURITY_CLIENT}
            vsomeip
            ${Boost_LIBRARIES}
            ${DL_LIBRARY}
            ${TEST_LINK_LIBRARIES}
        )
    endif()
endif()

##############################################################################
# payload-test
##############################################################################

if(NOT ${TESTS_BAT})
    set(TEST_LOCAL_PAYLOAD_NAME local_payload_test)

    set(TEST_PAYLOAD_SERVICE payload_test_service)
    add_executable(${TEST_PAYLOAD_SERVICE} payload_tests/${TEST_PAYLOAD_SERVICE}.cpp)
    target_link_libraries(${TEST_PAYLOAD_SERVICE}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    # Copy config file for service into $BUILDDIR/test
    set(TEST_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE local_${TEST_PAYLOAD_SERVICE}.json)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE}
        ${TEST_PAYLOAD_SERVICE}
    )

    # Copy bashscript to start service into $BUILDDIR/test
    set(TEST_LOCAL_PAYLOAD_SERVICE_START_SCRIPT local_${TEST_PAYLOAD_SERVICE}_start.sh)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_LOCAL_PAYLOAD_SERVICE_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_PAYLOAD_SERVICE_START_SCRIPT}
        ${TEST_PAYLOAD_SERVICE}
    )

    set(TEST_PAYLOAD_CLIENT payload_test_client)
    add_executable(${TEST_PAYLOAD_CLIENT}
        payload_tests/${TEST_PAYLOAD_CLIENT}.cpp
        payload_tests/stopwatch.cpp
    )
    target_link_libraries(${TEST_PAYLOAD_CLIENT}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    # Copy config file for client into $BUILDDIR/test
    set(TEST_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE local_${TEST_PAYLOAD_CLIENT}.json)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE}
        ${TEST_PAYLOAD_CLIENT}
    )

    # Copy bashscript to start client into $BUILDDIR/test
    set(TEST_LOCAL_PAYLOAD_CLIENT_START_SCRIPT local_${TEST_PAYLOAD_CLIENT}_start.sh)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_LOCAL_PAYLOAD_CLIENT_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_PAYLOAD_CLIENT_START_SCRIPT}
        ${TEST_PAYLOAD_CLIENT}
    )

    # Copy bashscript to start client and server $BUILDDIR/test
    set(TEST_LOCAL_PAYLOAD_STARTER local_payload_test_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_LOCAL_PAYLOAD_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_PAYLOAD_STARTER}
        ${TEST_PAYLOAD_CLIENT}
    )

    ##############################################################################
    set(TEST_EXTERNAL_LOCAL_PAYLOAD_NAME external_local_payload_test)
    set(TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE external_local_payload_test_service)

    # Copy config file for service into $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE ${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE}.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/payload_tests/conf/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CONFIG_FILE}
        ${TEST_PAYLOAD_SERVICE}
    )

    # Copy bashscript to start service into $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_START_SCRIPT ${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE}_start.sh)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_START_SCRIPT}
        ${TEST_PAYLOAD_SERVICE}
    )

    set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL external_local_payload_test_client_local)

    # Copy config file for client into $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL}.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/payload_tests/conf/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_CONFIG_FILE}
        ${TEST_PAYLOAD_CLIENT}
    )

    # Copy bashscript to start client into $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_START_SCRIPT ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL}_start.sh)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_START_SCRIPT}
        ${TEST_PAYLOAD_CLIENT}
    )

    # Copy bashscript to start client and server $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_NAME external_local_payload_test_client_local)
    set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_STARTER ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_NAME}_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_STARTER}
        ${TEST_PAYLOAD_CLIENT}
    )
    ##############################################################################
    set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_NAME external_local_payload_test_client_external)
    set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL external_local_payload_test_client_external)

    # Copy config file for client into $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_CONFIG_FILE ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL}.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/payload_tests/conf/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_CONFIG_FILE}
        ${TEST_PAYLOAD_CLIENT}
    )

    # Copy bashscript to start client into $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_START_SCRIPT
        ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL}_start.sh
        )
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_START_SCRIPT}
        ${TEST_PAYLOAD_CLIENT}
    )

    set(TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CLIENT_EXTERNAL external_local_payload_test_service_client_external)

    # Copy bashscript to start service into $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CLIENTT_EXTERNAL_START_SCRIPT
        ${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CLIENT_EXTERNAL}_start.sh)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CLIENTT_EXTERNAL_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CLIENTT_EXTERNAL_START_SCRIPT}
        ${TEST_PAYLOAD_SERVICE}
    )

    # Copy bashscript to start client and server $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_STARTER
        ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_NAME}_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_STARTER}
        ${TEST_PAYLOAD_CLIENT}
    )

    ##############################################################################
    # Copy bashscript to start client and server $BUILDDIR/test
    set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_NAME
        external_local_payload_test_client_local_and_external)
    set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_STARTER
        ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_NAME}_starter.sh)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_STARTER}
        ${TEST_PAYLOAD_CLIENT}
    )

    ##############################################################################
    set(TEST_LOCAL_PAYLOAD_HUGE_NAME local_payload_test_huge_payload)
    # Copy bashscript to start client and server $BUILDDIR/test
    set(TEST_LOCAL_PAYLOAD_HUGE_STARTER ${TEST_LOCAL_PAYLOAD_HUGE_NAME}_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_LOCAL_PAYLOAD_HUGE_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_PAYLOAD_HUGE_STARTER}
        ${TEST_PAYLOAD_CLIENT}
    )
endif()

##############################################################################
# big_payload_test
##############################################################################
if(NOT ${TESTS_BAT})
    set(TEST_BIG_PAYLOAD_NAME big_payload_test)

    set(TEST_BIG_PAYLOAD_SERVICE big_payload_test_service)
    add_executable(${TEST_BIG_PAYLOAD_SERVICE} big_payload_tests/${TEST_BIG_PAYLOAD_SERVICE}.cpp)
    target_link_libraries(${TEST_BIG_PAYLOAD_SERVICE}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    set(TEST_BIG_PAYLOAD_CLIENT big_payload_test_client)
    add_executable(${TEST_BIG_PAYLOAD_CLIENT} big_payload_tests/${TEST_BIG_PAYLOAD_CLIENT}.cpp)
    target_link_libraries(${TEST_BIG_PAYLOAD_CLIENT}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    # Copy config file for client and service into $BUILDDIR/test
    set(TEST_LOCAL_BIG_PAYLOAD_CONFIG_FILE ${TEST_BIG_PAYLOAD_NAME}_local.json)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/${TEST_LOCAL_BIG_PAYLOAD_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_BIG_PAYLOAD_CONFIG_FILE}
        ${TEST_BIG_PAYLOAD_SERVICE}
    )

    # Copy config file for client and service into $BUILDDIR/test
    set(TEST_QUEUE_LIMITED_LOCAL_BIG_PAYLOAD_CONFIG_FILE ${TEST_BIG_PAYLOAD_NAME}_local_queue_limited.json)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/${TEST_QUEUE_LIMITED_LOCAL_BIG_PAYLOAD_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_QUEUE_LIMITED_LOCAL_BIG_PAYLOAD_CONFIG_FILE}
        ${TEST_BIG_PAYLOAD_SERVICE}
    )

    # Copy config file for client and service into $BUILDDIR/test
    set(TEST_LOCAL_BIG_PAYLOAD_CONFIG_FILE_RANDOM ${TEST_BIG_PAYLOAD_NAME}_local_random.json)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/${TEST_LOCAL_BIG_PAYLOAD_CONFIG_FILE_RANDOM}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_BIG_PAYLOAD_CONFIG_FILE_RANDOM}
        ${TEST_BIG_PAYLOAD_SERVICE}
    )

    # Copy config file for client and service into $BUILDDIR/test
    set(TEST_LOCAL_BIG_PAYLOAD_CONFIG_FILE_LIMITED ${TEST_BIG_PAYLOAD_NAME}_local_limited.json)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/${TEST_LOCAL_BIG_PAYLOAD_CONFIG_FILE_LIMITED}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_BIG_PAYLOAD_CONFIG_FILE_LIMITED}
        ${TEST_BIG_PAYLOAD_SERVICE}
    )

    # Copy config file for client and service into $BUILDDIR/test
    set(TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE ${TEST_BIG_PAYLOAD_NAME}_tcp_client.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE}
        ${TEST_BIG_PAYLOAD_CLIENT}
    )

    # Copy config file for client and service into $BUILDDIR/test
    set(TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE ${TEST_BIG_PAYLOAD_NAME}_tcp_service.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE}
        ${TEST_BIG_PAYLOAD_SERVICE}
    )

    # Copy config file for client and service into $BUILDDIR/test
    set(TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_LIMITED ${TEST_BIG_PAYLOAD_NAME}_tcp_client_limited_general.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_LIMITED}.in
        ${PROJECT_BINARY_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_LIMITED}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_LIMITED}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_LIMITED}
        ${TEST_BIG_PAYLOAD_CLIENT}
    )

    # Copy config file for client and service into $BUILDDIR/test
    set(TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_LIMITED ${TEST_BIG_PAYLOAD_NAME}_tcp_service_limited_general.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_LIMITED}.in
        ${PROJECT_BINARY_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_LIMITED}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_LIMITED}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_LIMITED}
        ${TEST_BIG_PAYLOAD_SERVICE}
    )

    # Copy config file for client and service into $BUILDDIR/test
    set(TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_RANDOM ${TEST_BIG_PAYLOAD_NAME}_tcp_client_random.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_RANDOM}.in
        ${PROJECT_BINARY_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_RANDOM}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_RANDOM}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_RANDOM}
        ${TEST_BIG_PAYLOAD_CLIENT}
    )

    # Copy config file for client and service into $BUILDDIR/test
    set(TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_RANDOM ${TEST_BIG_PAYLOAD_NAME}_tcp_service_random.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_RANDOM}.in
        ${PROJECT_BINARY_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_RANDOM}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_RANDOM}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_RANDOM}
        ${TEST_BIG_PAYLOAD_SERVICE}
    )

    # Copy config file for client and service into $BUILDDIR/test
    set(TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED_GENERAL ${TEST_BIG_PAYLOAD_NAME}_tcp_client_queue_limited_general.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED_GENERAL}.in
        ${PROJECT_BINARY_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED_GENERAL}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED_GENERAL}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED_GENERAL}
        ${TEST_BIG_PAYLOAD_CLIENT}
    )

    # Copy config file for client and service into $BUILDDIR/test
    set(TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED_GENERAL ${TEST_BIG_PAYLOAD_NAME}_tcp_service_queue_limited_general.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED_GENERAL}.in
        ${PROJECT_BINARY_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED_GENERAL}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED_GENERAL}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED_GENERAL}
        ${TEST_BIG_PAYLOAD_SERVICE}
    )

    # Copy config file for client and service into $BUILDDIR/test
    set(TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED_SPECIFIC ${TEST_BIG_PAYLOAD_NAME}_tcp_client_queue_limited_specific.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED_SPECIFIC}.in
        ${PROJECT_BINARY_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED_SPECIFIC}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED_SPECIFIC}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_CONFIG_FILE_QUEUE_LIMITED_SPECIFIC}
        ${TEST_BIG_PAYLOAD_CLIENT}
    )

    # Copy config file for client and service into $BUILDDIR/test
    set(TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED_SPECIFIC ${TEST_BIG_PAYLOAD_NAME}_tcp_service_queue_limited_specific.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/conf/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED_SPECIFIC}.in
        ${PROJECT_BINARY_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED_SPECIFIC}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED_SPECIFIC}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_CONFIG_FILE_QUEUE_LIMITED_SPECIFIC}
        ${TEST_BIG_PAYLOAD_SERVICE}
    )

    # Copy bashscript to start client local to $BUILDDIR/test
    set(TEST_LOCAL_BIG_PAYLOAD_CLIENT_START_SCRIPT ${TEST_BIG_PAYLOAD_NAME}_client_local_start.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/big_payload_tests/${TEST_LOCAL_BIG_PAYLOAD_CLIENT_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_BIG_PAYLOAD_CLIENT_START_SCRIPT}
        ${TEST_BIG_PAYLOAD_CLIENT}
    )

    # Copy bashscript to start service local to $BUILDDIR/test
    set(TEST_LOCAL_BIG_PAYLOAD_SERVICE_START_SCRIPT ${TEST_BIG_PAYLOAD_NAME}_service_local_start.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/big_payload_tests/${TEST_LOCAL_BIG_PAYLOAD_SERVICE_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_BIG_PAYLOAD_SERVICE_START_SCRIPT}
        ${TEST_BIG_PAYLOAD_SERVICE}
    )

    # Copy bashscript to start client and server $BUILDDIR/test
    set(TEST_LOCAL_BIG_PAYLOAD_NAME big_payload_test_local)
    set(TEST_LOCAL_BIG_PAYLOAD_NAME_RANDOM big_payload_test_local_random)
    set(TEST_LOCAL_BIG_PAYLOAD_NAME_LIMITED big_payload_test_local_limited)
    set(TEST_LOCAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED big_payload_test_local_queue_limited)
    set(TEST_LOCAL_BIG_PAYLOAD_STARTER ${TEST_LOCAL_BIG_PAYLOAD_NAME}_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/big_payload_tests/${TEST_LOCAL_BIG_PAYLOAD_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_BIG_PAYLOAD_STARTER}
        ${TEST_BIG_PAYLOAD_SERVICE}
    )

    # Copy bashscript to start client for external test into $BUILDDIR/test
    set(TEST_EXTERNAL_BIG_PAYLOAD_NAME big_payload_test_external)
    set(TEST_EXTERNAL_BIG_PAYLOAD_NAME_RANDOM big_payload_test_external_random)
    set(TEST_EXTERNAL_BIG_PAYLOAD_NAME_LIMITED big_payload_test_external_limited)
    set(TEST_EXTERNAL_BIG_PAYLOAD_NAME_LIMITED_GENERAL big_payload_test_external_limited_general)
    set(TEST_EXTERNAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED_GENERAL big_payload_test_external_queue_limited_general)
    set(TEST_EXTERNAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED_SPECIFIC big_payload_test_external_queue_limited_specific)
    set(TEST_EXTERNAL_BIG_PAYLOAD_STARTER ${TEST_EXTERNAL_BIG_PAYLOAD_NAME}_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER}
        ${TEST_BIG_PAYLOAD_SERVICE}
    )

    # Copy bashscript to start server for external into $BUILDDIR/test
    set(TEST_EXTERNAL_BIG_PAYLOAD_NAME big_payload_test_external)
    set(TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_START_SCRIPT ${TEST_BIG_PAYLOAD_NAME}_service_external_start.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_BIG_PAYLOAD_SERVICE_START_SCRIPT}
        ${TEST_BIG_PAYLOAD_SERVICE}
    )

    # Copy bashscript to start client for external test into $BUILDDIR/test
    set(TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_START_SCRIPT ${TEST_BIG_PAYLOAD_NAME}_client_start.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/big_payload_tests/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_START_SCRIPT}
        ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_BIG_PAYLOAD_CLIENT_START_SCRIPT}
        ${TEST_BIG_PAYLOAD_CLIENT}
    )
endif()

##############################################################################
# client id tests
##############################################################################
if(NOT ${TESTS_BAT})
    set(TEST_CLIENT_ID_NAME client_id_test)
    set(TEST_CLIENT_ID_SERVICE ${TEST_CLIENT_ID_NAME}_service)
    add_executable(${TEST_CLIENT_ID_SERVICE} client_id_tests/${TEST_CLIENT_ID_NAME}_service.cpp)
    target_link_libraries(${TEST_CLIENT_ID_SERVICE}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    set(TEST_CLIENT_ID_UTILITY ${TEST_CLIENT_ID_NAME}_utility)
    add_executable(${TEST_CLIENT_ID_UTILITY}
        client_id_tests/${TEST_CLIENT_ID_UTILITY}.cpp
        ${PROJECT_SOURCE_DIR}/implementation/utility/src/utility.cpp)
    target_link_libraries(${TEST_CLIENT_ID_UTILITY}
        vsomeip
        vsomeip-cfg
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    # Copy config files for test into $BUILDDIR/test
    set(TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE
        ${TEST_CLIENT_ID_NAME}_diff_client_ids_diff_ports_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/conf/${TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${TEST_CLIENT_ID_SERVICE}
    )

    set(TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE
        ${TEST_CLIENT_ID_NAME}_diff_client_ids_diff_ports_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/conf/${TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_CLIENT_ID_SERVICE}
    )

    set(TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE
        ${TEST_CLIENT_ID_NAME}_diff_client_ids_same_ports_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/conf/${TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${TEST_CLIENT_ID_SERVICE}
    )

    set(TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE
        ${TEST_CLIENT_ID_NAME}_diff_client_ids_same_ports_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/conf/${TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_CLIENT_ID_SERVICE}
    )

    set(TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE
        ${TEST_CLIENT_ID_NAME}_diff_client_ids_partial_same_ports_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/conf/${TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}
        ${TEST_CLIENT_ID_SERVICE}
    )

    set(TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE
        ${TEST_CLIENT_ID_NAME}_diff_client_ids_partial_same_ports_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/conf/${TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/${TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_CLIENT_ID_SERVICE}
    )

    set(TEST_CLIENT_ID_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE
        ${TEST_CLIENT_ID_NAME}_same_client_ids_same_ports_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/conf/${TEST_CLIENT_ID_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/client_id_tests/${TEST_CLIENT_ID_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/${TEST_CLIENT_ID_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${TEST_CLIENT_ID_SERVICE}
    )

    set(TEST_CLIENT_ID_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE
        ${TEST_CLIENT_ID_NAME}_same_client_ids_same_ports_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/conf/${TEST_CLIENT_ID_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/client_id_tests/${TEST_CLIENT_ID_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/${TEST_CLIENT_ID_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_CLIENT_ID_SERVICE}
    )

    set(TEST_CLIENT_ID_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE
        ${TEST_CLIENT_ID_NAME}_same_client_ids_diff_ports_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/conf/${TEST_CLIENT_ID_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/client_id_tests/${TEST_CLIENT_ID_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/${TEST_CLIENT_ID_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${TEST_CLIENT_ID_SERVICE}
    )

    set(TEST_CLIENT_ID_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE
        ${TEST_CLIENT_ID_NAME}_same_client_ids_diff_ports_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/conf/${TEST_CLIENT_ID_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/client_id_tests/${TEST_CLIENT_ID_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/${TEST_CLIENT_ID_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_CLIENT_ID_SERVICE}
    )

    set(TEST_CLIENT_ID_UTILITY_CONFIG_FILE ${TEST_CLIENT_ID_NAME}_utility.json)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/${TEST_CLIENT_ID_UTILITY_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_UTILITY_CONFIG_FILE}
        ${TEST_CLIENT_ID_UTILITY}
    )
    set(TEST_CLIENT_ID_UTILITY_MASKED_511_CONFIG_FILE ${TEST_CLIENT_ID_NAME}_utility_masked_511.json)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/${TEST_CLIENT_ID_UTILITY_MASKED_511_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_UTILITY_MASKED_511_CONFIG_FILE}
        ${TEST_CLIENT_ID_UTILITY}
    )

    set(TEST_CLIENT_ID_UTILITY_MASKED_4095_CONFIG_FILE ${TEST_CLIENT_ID_NAME}_utility_masked_4095.json)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/${TEST_CLIENT_ID_UTILITY_MASKED_4095_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_UTILITY_MASKED_4095_CONFIG_FILE}
        ${TEST_CLIENT_ID_UTILITY}
    )

    set(TEST_CLIENT_ID_UTILITY_MASKED_127_CONFIG_FILE ${TEST_CLIENT_ID_NAME}_utility_masked_127.json)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/client_id_tests/${TEST_CLIENT_ID_UTILITY_MASKED_127_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_UTILITY_MASKED_127_CONFIG_FILE}
        ${TEST_CLIENT_ID_UTILITY}
    )

    # copy starter scripts into builddir
    set(TEST_CLIENT_ID_MASTER_STARTER ${TEST_CLIENT_ID_NAME}_master_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/client_id_tests/${TEST_CLIENT_ID_MASTER_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_MASTER_STARTER}
        ${TEST_CLIENT_ID_SERVICE}
    )
    set(TEST_CLIENT_ID_SLAVE_STARTER ${TEST_CLIENT_ID_NAME}_slave_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/client_id_tests/${TEST_CLIENT_ID_SLAVE_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_SLAVE_STARTER}
        ${TEST_CLIENT_ID_SERVICE}
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
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    # Copy config files for test into $BUILDDIR/test
    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_master_tcp.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_slave_tcp.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_master_tcp.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_slave_tcp.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_partial_same_ports_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_partial_same_ports_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_same_client_ids_same_ports_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_same_client_ids_same_ports_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_same_client_ids_diff_ports_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_same_client_ids_diff_ports_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_same_service_id_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_same_service_id_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_MASTER_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_autoconfig_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_MASTER_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_SLAVE_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_autoconfig_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_SLAVE_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    # copy starter scripts into builddir
    set(TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER ${TEST_SUBSCRIBE_NOTIFY_NAME}_master_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )
    set(TEST_SUBSCRIBE_NOTIFY_SLAVE_STARTER ${TEST_SUBSCRIBE_NOTIFY_NAME}_slave_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_SLAVE_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_SLAVE_STARTER}
        ${TEST_SUBSCRIBE_NOTIFY_SERVICE}
    )

    # subscribe_notify_test_one_event_two_eventgroups
    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME subscribe_notify_test_one_event_two_eventgroups)
    
    # service
    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SERVICE ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_service)
    add_executable(${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SERVICE}
        subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SERVICE}.cpp)
    target_link_libraries(${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SERVICE}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )
    # client
    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_CLIENT ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_client)
    add_executable(${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_CLIENT}
        subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_CLIENT}.cpp)
    target_link_libraries(${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_CLIENT}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_UDP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_udp_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_UDP_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_UDP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_UDP_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_UDP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_CLIENT}
    )
    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_tcp_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_TCP_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_TCP_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_CLIENT}
    )

    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_STARTER
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_master_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_STARTER}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SERVICE}
    )
    set(TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_STARTER
        ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_slave_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/subscribe_notify_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SLAVE_STARTER}
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
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )
    
    # Copy config files for test into $BUILDDIR/test
    set(TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_one_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_one_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_master_tcp.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_one_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE}
    )

    set(TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE
        ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_slave_tcp.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_one_tests/conf/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE}
    )

    # copy starter scripts into builddir
    set(TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_master_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER}
        ${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE}
    )
    set(TEST_SUBSCRIBE_NOTIFY_ONE_SLAVE_STARTER ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_slave_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/subscribe_notify_one_tests/${TEST_SUBSCRIBE_NOTIFY_ONE_SLAVE_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_ONE_SLAVE_STARTER}
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
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    # Copy config files for test into $BUILDDIR/test
    set(TEST_CPU_LOAD_SERVICE_MASTER_CONFIG_FILE ${TEST_CPU_LOAD_NAME}_service_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/cpu_load_tests/conf/${TEST_CPU_LOAD_SERVICE_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/cpu_load_tests/${TEST_CPU_LOAD_SERVICE_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/cpu_load_tests/${TEST_CPU_LOAD_SERVICE_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_CPU_LOAD_SERVICE_MASTER_CONFIG_FILE}
        ${TEST_CPU_LOAD_SERVICE}
    )
    set(TEST_CPU_LOAD_SERVICE_SLAVE_CONFIG_FILE ${TEST_CPU_LOAD_NAME}_service_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/cpu_load_tests/conf/${TEST_CPU_LOAD_SERVICE_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/cpu_load_tests/${TEST_CPU_LOAD_SERVICE_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/cpu_load_tests/${TEST_CPU_LOAD_SERVICE_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_CPU_LOAD_SERVICE_SLAVE_CONFIG_FILE}
        ${TEST_CPU_LOAD_SERVICE}
    )

    ##############################################################################
    set(TEST_CPU_LOAD_CLIENT cpu_load_test_client)
    add_executable(${TEST_CPU_LOAD_CLIENT}
        cpu_load_tests/${TEST_CPU_LOAD_CLIENT}.cpp
        cpu_load_tests/cpu_load_measurer.cpp
    )
    target_link_libraries(${TEST_CPU_LOAD_CLIENT}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    set(TEST_CPU_LOAD_CLIENT_MASTER_CONFIG_FILE ${TEST_CPU_LOAD_NAME}_client_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/cpu_load_tests/conf/${TEST_CPU_LOAD_CLIENT_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/cpu_load_tests/${TEST_CPU_LOAD_CLIENT_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/cpu_load_tests/${TEST_CPU_LOAD_CLIENT_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_CPU_LOAD_CLIENT_MASTER_CONFIG_FILE}
        ${TEST_CPU_LOAD_CLIENT}
    )
    set(TEST_CPU_LOAD_CLIENT_SLAVE_CONFIG_FILE ${TEST_CPU_LOAD_NAME}_client_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/cpu_load_tests/conf/${TEST_CPU_LOAD_CLIENT_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/cpu_load_tests/${TEST_CPU_LOAD_CLIENT_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/cpu_load_tests/${TEST_CPU_LOAD_CLIENT_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_CPU_LOAD_CLIENT_SLAVE_CONFIG_FILE}
        ${TEST_CPU_LOAD_CLIENT}
    )

    ##############################################################################
    # copy starter scripts into builddir
    set(TEST_CPU_LOAD_MASTER_STARTER ${TEST_CPU_LOAD_NAME}_master_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/cpu_load_tests/${TEST_CPU_LOAD_MASTER_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_CPU_LOAD_MASTER_STARTER}
        ${TEST_CPU_LOAD_SERVICE}
    )
    set(TEST_CPU_LOAD_SLAVE_STARTER ${TEST_CPU_LOAD_NAME}_slave_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/cpu_load_tests/${TEST_CPU_LOAD_SLAVE_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_CPU_LOAD_SLAVE_STARTER}
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
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )
 
    set(TEST_INITIAL_EVENT_CLIENT ${TEST_INITIAL_EVENT_NAME}_client)
    add_executable(${TEST_INITIAL_EVENT_CLIENT} initial_event_tests/${TEST_INITIAL_EVENT_NAME}_client.cpp)
    target_link_libraries(${TEST_INITIAL_EVENT_CLIENT}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    set(TEST_INITIAL_EVENT_AVAILABILITY_CHECKER ${TEST_INITIAL_EVENT_NAME}_availability_checker)
    add_executable(${TEST_INITIAL_EVENT_AVAILABILITY_CHECKER} initial_event_tests/${TEST_INITIAL_EVENT_NAME}_availability_checker.cpp)
    target_link_libraries(${TEST_INITIAL_EVENT_AVAILABILITY_CHECKER}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    set(TEST_INITIAL_EVENT_STOP_SERVICE ${TEST_INITIAL_EVENT_NAME}_stop_service)
    add_executable(${TEST_INITIAL_EVENT_STOP_SERVICE} initial_event_tests/${TEST_INITIAL_EVENT_NAME}_stop_service.cpp)
    target_link_libraries(${TEST_INITIAL_EVENT_STOP_SERVICE}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    # Copy config files for test into $BUILDDIR/test
    set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    # Copy config files for test into $BUILDDIR/test
    set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_master_tcp.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_slave_tcp.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SLAVE_TCP_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_master_tcp.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_slave_tcp.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_SLAVE_TCP_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_partial_same_ports_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_partial_same_ports_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_same_client_ids_same_ports_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/conf/${TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_MASTER_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_same_client_ids_same_ports_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/conf/${TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_SAME_IDS_SAME_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_same_client_ids_diff_ports_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/conf/${TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_MASTER_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_same_client_ids_diff_ports_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/conf/${TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_SAME_IDS_DIFF_PORTS_SLAVE_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_same_service_id_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    set(TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_CONFIG_FILE
        ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_same_service_id_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/conf/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_SLAVE_CONFIG_FILE}
        ${TEST_INITIAL_EVENT_SERVICE}
    )

    # copy starter scripts into builddir
    set(TEST_INITIAL_EVENT_MASTER_STARTER ${TEST_INITIAL_EVENT_NAME}_master_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_MASTER_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER}
        ${TEST_INITIAL_EVENT_SERVICE}
    )
    set(TEST_INITIAL_EVENT_SLAVE_STARTER ${TEST_INITIAL_EVENT_NAME}_slave_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/initial_event_tests/${TEST_INITIAL_EVENT_SLAVE_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_SLAVE_STARTER}
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
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    set(TEST_OFFER_CLIENT ${TEST_OFFER_NAME}_client)
    add_executable(${TEST_OFFER_CLIENT} offer_tests/${TEST_OFFER_NAME}_client.cpp)
    target_link_libraries(${TEST_OFFER_CLIENT}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    set(TEST_OFFER_SERVICE_EXTERNAL ${TEST_OFFER_NAME}_service_external)
    add_executable(${TEST_OFFER_SERVICE_EXTERNAL} offer_tests/${TEST_OFFER_NAME}_service_external.cpp)
    target_link_libraries(${TEST_OFFER_SERVICE_EXTERNAL}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    set(TEST_OFFER_EXTERNAL_SD_MESSAGE_SENDER ${TEST_OFFER_NAME}_external_sd_msg_sender)
    add_executable(${TEST_OFFER_EXTERNAL_SD_MESSAGE_SENDER} offer_tests/${TEST_OFFER_NAME}_external_sd_msg_sender.cpp)
    target_link_libraries(${TEST_OFFER_EXTERNAL_SD_MESSAGE_SENDER}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    # client and service for offer test big sd msg
    set(TEST_OFFER_BIG_NAME offer_test_big_sd_msg)
    set(TEST_OFFER_BIG_SERVICE ${TEST_OFFER_BIG_NAME}_service)
    add_executable(${TEST_OFFER_BIG_SERVICE} offer_tests/${TEST_OFFER_BIG_NAME}_service.cpp)
    target_link_libraries(${TEST_OFFER_BIG_SERVICE}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    set(TEST_OFFER_BIG_CLIENT ${TEST_OFFER_BIG_NAME}_client)
    add_executable(${TEST_OFFER_BIG_CLIENT} offer_tests/${TEST_OFFER_BIG_NAME}_client.cpp)
    target_link_libraries(${TEST_OFFER_BIG_CLIENT}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    # copy starter scripts into builddir
    set(TEST_OFFER_LOCAL_STARTER ${TEST_OFFER_NAME}_local_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/offer_tests/${TEST_OFFER_LOCAL_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_OFFER_LOCAL_STARTER}
        ${TEST_OFFER_SERVICE}
    )

    # Copy config file for local test into $BUILDDIR/test
    set(TEST_OFFER_LOCAL_CONFIG_FILE ${TEST_OFFER_NAME}_local.json)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/offer_tests/${TEST_OFFER_LOCAL_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_OFFER_LOCAL_CONFIG_FILE}
        ${TEST_OFFER_SERVICE}
    )

    # generate and copy json files into builddir for external test
    set(TEST_OFFER_SLAVE_CONFIG_FILE ${TEST_OFFER_NAME}_external_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/offer_tests/conf/${TEST_OFFER_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/offer_tests/${TEST_OFFER_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/offer_tests/${TEST_OFFER_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_OFFER_SLAVE_CONFIG_FILE}
        ${TEST_OFFER_SERVICE}
    )

    set(TEST_OFFER_MASTER_CONFIG_FILE ${TEST_OFFER_NAME}_external_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/offer_tests/conf/${TEST_OFFER_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/offer_tests/${TEST_OFFER_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/offer_tests/${TEST_OFFER_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_OFFER_MASTER_CONFIG_FILE}
        ${TEST_OFFER_SERVICE}
    )

    # generate and copy json files into builddir for big SD message test
    set(TEST_OFFER_BIG_SLAVE_CONFIG_FILE ${TEST_OFFER_BIG_NAME}_slave.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/offer_tests/conf/${TEST_OFFER_BIG_SLAVE_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/offer_tests/${TEST_OFFER_BIG_SLAVE_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/offer_tests/${TEST_OFFER_BIG_SLAVE_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_OFFER_BIG_SLAVE_CONFIG_FILE}
        ${TEST_OFFER_BIG_SERVICE}
    )

    set(TEST_OFFER_BIG_MASTER_CONFIG_FILE ${TEST_OFFER_BIG_NAME}_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/offer_tests/conf/${TEST_OFFER_BIG_MASTER_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/offer_tests/${TEST_OFFER_BIG_MASTER_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/offer_tests/${TEST_OFFER_BIG_MASTER_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_OFFER_BIG_MASTER_CONFIG_FILE}
        ${TEST_OFFER_BIG_SERVICE}
    )

    # Copy starter scripts for external test to $BUILDDIR/test
    set(TEST_OFFER_EXTERNAL_MASTER_STARTER ${TEST_OFFER_NAME}_external_master_starter.sh)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/offer_tests/conf/${TEST_OFFER_EXTERNAL_MASTER_STARTER}.in
        ${PROJECT_BINARY_DIR}/test/offer_tests/${TEST_OFFER_EXTERNAL_MASTER_STARTER}
        @ONLY)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/offer_tests/${TEST_OFFER_EXTERNAL_MASTER_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_OFFER_EXTERNAL_MASTER_STARTER}
        ${TEST_OFFER_SERVICE}
    )
    set(TEST_OFFER_EXTERNAL_SLAVE_STARTER ${TEST_OFFER_NAME}_external_slave_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/offer_tests/${TEST_OFFER_EXTERNAL_SLAVE_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_OFFER_EXTERNAL_SLAVE_STARTER}
        ${TEST_OFFER_SERVICE}
    )

    # Copy starter scripts for external test to $BUILDDIR/test
    set(TEST_OFFER_BIG_MASTER_STARTER ${TEST_OFFER_BIG_NAME}_master_starter.sh)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/offer_tests/conf/${TEST_OFFER_BIG_MASTER_STARTER}.in
        ${PROJECT_BINARY_DIR}/test/offer_tests/${TEST_OFFER_BIG_MASTER_STARTER}
        @ONLY)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/offer_tests/${TEST_OFFER_BIG_MASTER_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_OFFER_BIG_MASTER_STARTER}
        ${TEST_OFFER_BIG_SERVICE}
    )
    # Copy starter scripts for external test to $BUILDDIR/test
    set(TEST_OFFER_BIG_EXTERNAL_SLAVE_STARTER ${TEST_OFFER_BIG_NAME}_slave_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/offer_tests/${TEST_OFFER_BIG_EXTERNAL_SLAVE_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_OFFER_BIG_EXTERNAL_SLAVE_STARTER}
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
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    set(TEST_OFFERED_SERVICES_INFO_CLIENT ${TEST_OFFERED_SERVICES_INFO_NAME}_client)
    add_executable(${TEST_OFFERED_SERVICES_INFO_CLIENT} offered_services_info_test/${TEST_OFFERED_SERVICES_INFO_NAME}_client.cpp)
    target_link_libraries(${TEST_OFFERED_SERVICES_INFO_CLIENT}
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    # copy starter scripts into builddir
    set(TEST_OFFERED_SERVICES_INFO_LOCAL_STARTER ${TEST_OFFERED_SERVICES_INFO_NAME}_local_starter.sh)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/offered_services_info_test/${TEST_OFFERED_SERVICES_INFO_LOCAL_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_OFFERED_SERVICES_INFO_LOCAL_STARTER}
        ${TEST_OFFERED_SERVICES_INFO_SERVICE}
    )

    # Copy config file for local test into $BUILDDIR/test
    set(TEST_OFFERED_SERVICES_INFO_LOCAL_CONFIG_FILE ${TEST_OFFERED_SERVICES_INFO_NAME}_local.json)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/offered_services_info_test/${TEST_OFFERED_SERVICES_INFO_LOCAL_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_OFFERED_SERVICES_INFO_LOCAL_CONFIG_FILE}
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
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    file(GLOB sd_sources
        "../implementation/service_discovery/src/*entry*.cpp"
        "../implementation/service_discovery/src/*option*.cpp"
        "../implementation/service_discovery/src/*message*.cpp"
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
        vsomeip
        vsomeip-sd
    )

    # copy starter scripts into builddir
    set(TEST_PENDING_SUBSCRIPTION_MASTER_STARTER ${TEST_PENDING_SUBSCRIPTION_NAME}_master_starter.sh)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/pending_subscription_tests/conf/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER}.in
        ${PROJECT_BINARY_DIR}/test/pending_subscription_tests/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER}
        @ONLY)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/pending_subscription_tests/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER}
        ${TEST_PENDING_SUBSCRIPTION_SERVICE}
    )

    # Copy config file for local test into $BUILDDIR/test
    set(TEST_PENDING_SUBSCRIPTION_CONFIG_FILE ${TEST_PENDING_SUBSCRIPTION_NAME}_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/pending_subscription_tests/conf/${TEST_PENDING_SUBSCRIPTION_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/pending_subscription_tests/${TEST_PENDING_SUBSCRIPTION_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/pending_subscription_tests/${TEST_PENDING_SUBSCRIPTION_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_PENDING_SUBSCRIPTION_CONFIG_FILE}
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
        vsomeip
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
    )

    file(GLOB sd_sources
        "../implementation/service_discovery/src/*entry*.cpp"
        "../implementation/service_discovery/src/*option*.cpp"
        "../implementation/service_discovery/src/*message*.cpp"
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
        vsomeip
        vsomeip-sd
    )

    # copy starter scripts into builddir
    set(TEST_MALICIOUS_DATA_MASTER_STARTER ${TEST_MALICIOUS_DATA_NAME}_master_starter.sh)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/malicious_data_tests/conf/${TEST_MALICIOUS_DATA_MASTER_STARTER}.in
        ${PROJECT_BINARY_DIR}/test/malicious_data_tests/${TEST_MALICIOUS_DATA_MASTER_STARTER}
        @ONLY)
    copy_to_builddir(${PROJECT_SOURCE_DIR}/test/malicious_data_tests/${TEST_MALICIOUS_DATA_MASTER_STARTER}
        ${PROJECT_BINARY_DIR}/test/${TEST_MALICIOUS_DATA_MASTER_STARTER}
        ${TEST_MALICIOUS_DATA_SERVICE}
    )

    # Copy config file for local test into $BUILDDIR/test
    set(TEST_MALICIOUS_DATA_CONFIG_FILE ${TEST_MALICIOUS_DATA_NAME}_master.json)
    configure_file(
        ${PROJECT_SOURCE_DIR}/test/malicious_data_tests/conf/${TEST_MALICIOUS_DATA_CONFIG_FILE}.in
        ${PROJECT_BINARY_DIR}/test/malicious_data_tests/${TEST_MALICIOUS_DATA_CONFIG_FILE}
        @ONLY)
    copy_to_builddir(
        ${PROJECT_SOURCE_DIR}/test/malicious_data_tests/${TEST_MALICIOUS_DATA_CONFIG_FILE}
        ${PROJECT_BINARY_DIR}/test/${TEST_MALICIOUS_DATA_CONFIG_FILE}
        ${TEST_MALICIOUS_DATA_SERVICE}
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
else()
    add_dependencies(${TEST_LOCAL_ROUTING_SERVICE} gtest)
    add_dependencies(${TEST_LOCAL_ROUTING_CLIENT} gtest)
endif()

##############################################################################
# Add tests to the target build_tests
##############################################################################

if(NOT ${TESTS_BAT})
    add_dependencies(build_tests ${TEST_CONFIGURATION})
    add_dependencies(build_tests ${TEST_APPLICATION})
    add_dependencies(build_tests ${TEST_APPLICATION_SINGLE_PROCESS_NAME})
    add_dependencies(build_tests ${TEST_APPLICATION_AVAILABILITY_NAME})
    add_dependencies(build_tests ${TEST_MAGIC_COOKIES_CLIENT})
    add_dependencies(build_tests ${TEST_MAGIC_COOKIES_SERVICE})
    add_dependencies(build_tests ${TEST_HEADER_FACTORY})
    add_dependencies(build_tests ${TEST_HEADER_FACTORY_CLIENT})
    add_dependencies(build_tests ${TEST_HEADER_FACTORY_SERVICE})
    add_dependencies(build_tests ${TEST_LOCAL_ROUTING_SERVICE})
    add_dependencies(build_tests ${TEST_LOCAL_ROUTING_CLIENT})
    add_dependencies(build_tests ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE})
    add_dependencies(build_tests ${TEST_PAYLOAD_SERVICE})
    add_dependencies(build_tests ${TEST_PAYLOAD_CLIENT})
    add_dependencies(build_tests ${TEST_BIG_PAYLOAD_SERVICE})
    add_dependencies(build_tests ${TEST_BIG_PAYLOAD_CLIENT})
    add_dependencies(build_tests ${TEST_CLIENT_ID_SERVICE})
    add_dependencies(build_tests ${TEST_CLIENT_ID_UTILITY})
    add_dependencies(build_tests ${TEST_SUBSCRIBE_NOTIFY_SERVICE})
    add_dependencies(build_tests ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_SERVICE})
    add_dependencies(build_tests ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_CLIENT})
    add_dependencies(build_tests ${TEST_SUBSCRIBE_NOTIFY_ONE_SERVICE})
    add_dependencies(build_tests ${TEST_CPU_LOAD_SERVICE})
    add_dependencies(build_tests ${TEST_CPU_LOAD_CLIENT})
    add_dependencies(build_tests ${TEST_INITIAL_EVENT_SERVICE})
    add_dependencies(build_tests ${TEST_INITIAL_EVENT_CLIENT})
    add_dependencies(build_tests ${TEST_INITIAL_EVENT_AVAILABILITY_CHECKER})
    add_dependencies(build_tests ${TEST_INITIAL_EVENT_STOP_SERVICE})
    add_dependencies(build_tests ${TEST_OFFER_SERVICE})
    add_dependencies(build_tests ${TEST_OFFER_CLIENT})
    add_dependencies(build_tests ${TEST_OFFER_SERVICE_EXTERNAL})
    add_dependencies(build_tests ${TEST_OFFER_EXTERNAL_SD_MESSAGE_SENDER})
    add_dependencies(build_tests ${TEST_OFFER_BIG_SERVICE})
    add_dependencies(build_tests ${TEST_OFFER_BIG_CLIENT})
    add_dependencies(build_tests ${TEST_RESTART_ROUTING_SERVICE})
    add_dependencies(build_tests ${TEST_RESTART_ROUTING_CLIENT})
    if (${TEST_SECURITY})
        add_dependencies(build_tests ${TEST_SECURITY_SERVICE})
        add_dependencies(build_tests ${TEST_SECURITY_CLIENT})
    endif()
    add_dependencies(build_tests ${TEST_OFFERED_SERVICES_INFO_CLIENT})
    add_dependencies(build_tests ${TEST_OFFERED_SERVICES_INFO_SERVICE})
    add_dependencies(build_tests ${TEST_PENDING_SUBSCRIPTION_SERVICE})
    add_dependencies(build_tests ${TEST_PENDING_SUBSCRIPTION_CLIENT})
    add_dependencies(build_tests ${TEST_MALICIOUS_DATA_SERVICE})
    add_dependencies(build_tests ${TEST_MALICIOUS_DATA_CLIENT})
else()
    add_dependencies(build_tests ${TEST_LOCAL_ROUTING_SERVICE})
    add_dependencies(build_tests ${TEST_LOCAL_ROUTING_CLIENT})
endif()



##############################################################################
# Add tests
##############################################################################

if(NOT ${TESTS_BAT})
    add_test(NAME ${TEST_CONFIGURATION}
        COMMAND ${TEST_CONFIGURATION}
    )

    # application test
    add_test(NAME ${TEST_APPLICATION}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_APPLICATION_STARTER}
    )
    set_tests_properties(${TEST_APPLICATION} PROPERTIES TIMEOUT 80)
    add_test(NAME ${TEST_APPLICATION_SINGLE_PROCESS_NAME}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_APPLICATION_SINGLE_PROCESS_STARTER}
    )
    set_tests_properties(${TEST_APPLICATION_SINGLE_PROCESS_NAME} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_APPLICATION_AVAILABILITY_NAME}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_APPLICATION_AVAILABILITY_STARTER}
    )
    set_tests_properties(${TEST_APPLICATION_AVAILABILITY_NAME} PROPERTIES TIMEOUT 120)

    # magic cookies test
    add_test(NAME ${TEST_MAGIC_COOKIES_NAME}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_MAGIC_COOKIES_STARTER}
    )
    set_tests_properties(${TEST_MAGIC_COOKIES_NAME} PROPERTIES TIMEOUT 250)

    # Header/Factory tets
    add_test(NAME ${TEST_HEADER_FACTORY_NAME} COMMAND ${TEST_HEADER_FACTORY})
    add_test(NAME ${TEST_HEADER_FACTORY_NAME}_send_receive
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_HEADER_FACTORY_STARTER}
    )

    # Routing tests
    add_test(NAME ${TEST_LOCAL_ROUTING_NAME}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_ROUTING_STARTER}
    )

    add_test(NAME ${TEST_EXTERNAL_LOCAL_ROUTING_NAME}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_ROUTING_STARTER}
    )
    set_tests_properties(${TEST_EXTERNAL_LOCAL_ROUTING_NAME} PROPERTIES TIMEOUT 120)

    # Payload tests
    add_test(NAME ${TEST_LOCAL_PAYLOAD_NAME}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_PAYLOAD_STARTER}
    )
    add_test(NAME ${TEST_LOCAL_PAYLOAD_HUGE_NAME}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_PAYLOAD_HUGE_STARTER}
    )
    set_tests_properties(${TEST_LOCAL_PAYLOAD_HUGE_NAME} PROPERTIES TIMEOUT 480)
    add_test(NAME ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_NAME}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_STARTER}
    )
    add_test(NAME ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_NAME}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_STARTER}
    )
    set_tests_properties(${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_NAME} PROPERTIES TIMEOUT 480)
    add_test(NAME ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_NAME}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_STARTER}
    )
    set_tests_properties(${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_NAME} PROPERTIES TIMEOUT 480)

    # big payload tests
    add_test(NAME ${TEST_LOCAL_BIG_PAYLOAD_NAME}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_BIG_PAYLOAD_STARTER}
    )
    set_tests_properties(${TEST_LOCAL_BIG_PAYLOAD_NAME} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_LOCAL_BIG_PAYLOAD_NAME_RANDOM}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_BIG_PAYLOAD_STARTER} RANDOM
    )
    set_tests_properties(${TEST_LOCAL_BIG_PAYLOAD_NAME_RANDOM} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_LOCAL_BIG_PAYLOAD_NAME_LIMITED}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_BIG_PAYLOAD_STARTER} LIMITED
    )
    set_tests_properties(${TEST_LOCAL_BIG_PAYLOAD_NAME_LIMITED} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_EXTERNAL_BIG_PAYLOAD_NAME}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER}
    )
    set_tests_properties(${TEST_EXTERNAL_BIG_PAYLOAD_NAME} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_EXTERNAL_BIG_PAYLOAD_NAME_RANDOM}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER} RANDOM
    )
    set_tests_properties(${TEST_EXTERNAL_BIG_PAYLOAD_NAME_RANDOM} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_EXTERNAL_BIG_PAYLOAD_NAME_LIMITED}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER} LIMITED
    )
    set_tests_properties(${TEST_EXTERNAL_BIG_PAYLOAD_NAME_LIMITED} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_EXTERNAL_BIG_PAYLOAD_NAME_LIMITED_GENERAL}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER} LIMITEDGENERAL
    )
    set_tests_properties(${TEST_EXTERNAL_BIG_PAYLOAD_NAME_LIMITED_GENERAL} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_LOCAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_BIG_PAYLOAD_STARTER} QUEUELIMITEDGENERAL
    )
    set_tests_properties(${TEST_LOCAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_EXTERNAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED_GENERAL}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER} QUEUELIMITEDGENERAL
    )
    set_tests_properties(${TEST_EXTERNAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED_GENERAL} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_EXTERNAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED_SPECIFIC}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_BIG_PAYLOAD_STARTER} QUEUELIMITEDSPECIFIC
    )
    set_tests_properties(${TEST_EXTERNAL_BIG_PAYLOAD_NAME_QUEUE_LIMITED_SPECIFIC} PROPERTIES TIMEOUT 120)

    # client id tests
    add_test(NAME ${TEST_CLIENT_ID_NAME}_diff_client_ids_diff_ports
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_MASTER_STARTER} ${TEST_CLIENT_ID_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_CLIENT_ID_NAME}_diff_client_ids_diff_ports PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_CLIENT_ID_NAME}_diff_client_ids_same_ports
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_MASTER_STARTER} ${TEST_CLIENT_ID_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_CLIENT_ID_NAME}_diff_client_ids_same_ports PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_CLIENT_ID_NAME}_diff_client_ids_partial_same_ports
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_MASTER_STARTER} ${TEST_CLIENT_ID_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_CLIENT_ID_NAME}_diff_client_ids_partial_same_ports PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_CLIENT_ID_UTILITY}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_UTILITY})
    set_property(TEST ${TEST_CLIENT_ID_UTILITY}
        APPEND PROPERTY ENVIRONMENT
        "VSOMEIP_CONFIGURATION=${TEST_CLIENT_ID_UTILITY_CONFIG_FILE}")
    set_tests_properties(${TEST_CLIENT_ID_UTILITY} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_CLIENT_ID_UTILITY}_masked_511
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_UTILITY})
    set_property(TEST ${TEST_CLIENT_ID_UTILITY}_masked_511
        APPEND PROPERTY ENVIRONMENT
        "VSOMEIP_CONFIGURATION=${TEST_CLIENT_ID_UTILITY_MASKED_511_CONFIG_FILE}")
    set_tests_properties(${TEST_CLIENT_ID_UTILITY} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_CLIENT_ID_UTILITY}_masked_4095
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_UTILITY})
    set_property(TEST ${TEST_CLIENT_ID_UTILITY}_masked_4095
        APPEND PROPERTY ENVIRONMENT
        "VSOMEIP_CONFIGURATION=${TEST_CLIENT_ID_UTILITY_MASKED_4095_CONFIG_FILE}")
    set_tests_properties(${TEST_CLIENT_ID_UTILITY} PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_CLIENT_ID_UTILITY}_masked_127
    COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_CLIENT_ID_UTILITY})
    set_property(TEST ${TEST_CLIENT_ID_UTILITY}_masked_127
        APPEND PROPERTY ENVIRONMENT
        "VSOMEIP_CONFIGURATION=${TEST_CLIENT_ID_UTILITY_MASKED_127_CONFIG_FILE}")
    set_tests_properties(${TEST_CLIENT_ID_UTILITY} PROPERTIES TIMEOUT 120)

    # subscribe notify tests
    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_tcp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} TCP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_both_tcp_and_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} TCP_AND_UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_prefer_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} PREFER_UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_prefer_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_prefer_tcp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} PREFER_TCP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_prefer_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_tcp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} TCP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_both_tcp_and_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} TCP_AND_UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_prefer_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} PREFER_UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_prefer_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_prefer_tcp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} PREFER_TCP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_same_ports_prefer_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_partial_same_ports_both_tcp_and_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} TCP_AND_UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_partial_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)
    
    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_same_service_id_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE} SAME_SERVICE_ID)
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_same_service_id_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_autoconfig_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_MASTER_STARTER} UDP ${TEST_SUBSCRIBE_NOTIFY_DIFF_IDS_DIFF_PORTS_AUTOCONFIG_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_NAME}_diff_client_ids_diff_ports_same_service_id_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_STARTER} UDP ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_tcp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_STARTER} TCP ${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_ONE_EVENT_TWO_EVENTGROUPS_NAME}_tcp PROPERTIES TIMEOUT 120)

    # subscribe notify one id tests
    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER} UDP ${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_tcp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER} TCP ${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_both_tcp_and_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER} TCP_AND_UDP ${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_prefer_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER} PREFER_UDP ${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_prefer_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_prefer_tcp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SUBSCRIBE_NOTIFY_ONE_MASTER_STARTER} PREFER_TCP ${TEST_SUBSCRIBE_NOTIFY_ONE_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_SUBSCRIBE_NOTIFY_ONE_NAME}_diff_client_ids_diff_ports_prefer_tcp PROPERTIES TIMEOUT 120)

    # cpu load tests
    add_test(NAME ${TEST_CPU_LOAD_NAME}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_CPU_LOAD_MASTER_STARTER}
    )
    set_tests_properties(${TEST_CPU_LOAD_NAME} PROPERTIES TIMEOUT 3000)

    # initial event tests
    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} UDP ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_tcp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} TCP ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_both_tcp_and_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} TCP_AND_UDP ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_prefer_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} PREFER_UDP ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_prefer_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_prefer_tcp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} PREFER_TCP ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_prefer_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} UDP ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_tcp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} TCP ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_both_tcp_and_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} TCP_AND_UDP ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_prefer_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} PREFER_UDP ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_prefer_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_prefer_tcp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} PREFER_TCP ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE})
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_same_ports_prefer_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_partial_same_ports_both_tcp_and_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} TCP_AND_UDP ${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE})
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_partial_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_same_service_id_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} UDP ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE} SAME_SERVICE_ID)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_diff_client_ids_diff_ports_same_service_id_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} UDP ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE} MULTIPLE_EVENTS)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_tcp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} TCP ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE} MULTIPLE_EVENTS)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_both_tcp_and_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} TCP_AND_UDP ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE} MULTIPLE_EVENTS)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_prefer_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} PREFER_UDP ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE} MULTIPLE_EVENTS)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_prefer_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_prefer_tcp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} PREFER_TCP ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE} MULTIPLE_EVENTS)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_prefer_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} UDP ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE} MULTIPLE_EVENTS)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_tcp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} TCP ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE} MULTIPLE_EVENTS)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_both_tcp_and_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} TCP_AND_UDP ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE} MULTIPLE_EVENTS)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_prefer_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} PREFER_UDP ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE} MULTIPLE_EVENTS)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_prefer_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_prefer_tcp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} PREFER_TCP ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE} MULTIPLE_EVENTS)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_same_ports_prefer_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_partial_same_ports_both_tcp_and_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} TCP_AND_UDP ${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE} MULTIPLE_EVENTS)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_partial_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_same_service_id_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} UDP ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE} SAME_SERVICE_ID MULTIPLE_EVENTS)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_diff_client_ids_diff_ports_same_service_id_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} UDP ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE} MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_tcp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} TCP ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE} MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_both_tcp_and_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} TCP_AND_UDP ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE} MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_prefer_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} PREFER_UDP ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_CONFIG_FILE} MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_prefer_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_prefer_tcp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} PREFER_TCP ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_MASTER_TCP_CONFIG_FILE} MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_prefer_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} UDP ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE} MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_tcp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} TCP ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE} MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_both_tcp_and_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} TCP_AND_UDP ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE} MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_prefer_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} PREFER_UDP ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_CONFIG_FILE} MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_prefer_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_prefer_tcp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} PREFER_TCP ${TEST_INITIAL_EVENT_DIFF_IDS_SAME_PORTS_MASTER_TCP_CONFIG_FILE} MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_same_ports_prefer_tcp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_partial_same_ports_both_tcp_and_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} TCP_AND_UDP ${TEST_INITIAL_EVENT_DIFF_IDS_PARTIAL_SAME_PORTS_MASTER_CONFIG_FILE} MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_partial_same_ports_both_tcp_and_udp PROPERTIES TIMEOUT 120)

    add_test(NAME ${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_same_service_id_udp
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_INITIAL_EVENT_MASTER_STARTER} UDP ${TEST_INITIAL_EVENT_DIFF_IDS_DIFF_PORTS_SAME_SERVICEID_MASTER_CONFIG_FILE} SAME_SERVICE_ID MULTIPLE_EVENTS SUBSCRIBE_ON_AVAILABILITY)
    set_tests_properties(${TEST_INITIAL_EVENT_NAME}_multiple_events_subscribe_on_availability_diff_client_ids_diff_ports_same_service_id_udp PROPERTIES TIMEOUT 120)

    # offer tests
    add_test(NAME ${TEST_OFFER_NAME}_local
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_OFFER_LOCAL_STARTER})
    set_tests_properties(${TEST_OFFER_NAME}_local PROPERTIES TIMEOUT 180)
    add_test(NAME ${TEST_OFFER_NAME}_external
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_OFFER_EXTERNAL_MASTER_STARTER})
    set_tests_properties(${TEST_OFFER_NAME}_local PROPERTIES TIMEOUT 360)
    add_test(NAME ${TEST_OFFER_BIG_NAME}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_OFFER_BIG_MASTER_STARTER})
    set_tests_properties(${TEST_OFFER_BIG_NAME} PROPERTIES TIMEOUT 360)
    
    # offered services info tets
    add_test(NAME ${TEST_OFFERED_SERVICES_INFO_NAME}_local
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_OFFERED_SERVICES_INFO_LOCAL_STARTER})
    set_tests_properties(${TEST_OFFERED_SERVICES_INFO_NAME}_local PROPERTIES TIMEOUT 180)  
    
    # Restart-Routing tests
    add_test(NAME ${TEST_RESTART_ROUTING_NAME}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_RESTART_ROUTING_STARTER}
    )
    if (${TEST_SECURITY})
        # Security tests
        add_test(NAME ${TEST_SECURITY_NAME}
            COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_SECURITY_SERVICE_START_SCRIPT}
        )
    endif()

    # pending subscriptions test
    add_test(NAME ${TEST_PENDING_SUBSCRIPTION_NAME}_subscribe
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER} SUBSCRIBE)
    set_tests_properties(${TEST_PENDING_SUBSCRIPTION_NAME}_subscribe PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_PENDING_SUBSCRIPTION_NAME}_alternating_subscribe_unsubscribe
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER} SUBSCRIBE_UNSUBSCRIBE)
    set_tests_properties(${TEST_PENDING_SUBSCRIPTION_NAME}_alternating_subscribe_unsubscribe PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_PENDING_SUBSCRIPTION_NAME}_unsubscribe
    COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER} UNSUBSCRIBE)
    set_tests_properties(${TEST_PENDING_SUBSCRIPTION_NAME}_unsubscribe PROPERTIES TIMEOUT 180)

    add_test(NAME ${TEST_PENDING_SUBSCRIPTION_NAME}_alternating_subscribe_unsubscribe_nack
    COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_PENDING_SUBSCRIPTION_MASTER_STARTER} SUBSCRIBE_UNSUBSCRIBE_NACK)
    set_tests_properties(${TEST_PENDING_SUBSCRIPTION_NAME}_alternating_subscribe_unsubscribe_nack PROPERTIES TIMEOUT 180)

    # malicious data test
    add_test(NAME ${TEST_MALICIOUS_DATA_NAME}
    COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_MALICIOUS_DATA_MASTER_STARTER})
    set_tests_properties(${TEST_MALICIOUS_DATA_NAME} PROPERTIES TIMEOUT 180)
else()
    # Routing tests
    add_test(NAME ${TEST_LOCAL_ROUTING_NAME}
        COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_ROUTING_STARTER}
    )
endif()
