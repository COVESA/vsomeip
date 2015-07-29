# Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required (VERSION 2.8.1)

# Add the gtest header files to the include files
include_directories(
    .
    ${gtest_SOURCE_DIR}/include
)

set(TEST_LINK_LIBRARIES gtest)

# Function to copy files into the build directory (or anywhere else)
# SOURCE_PATH: Path to the file which should be copied
# DESTINATION_PATH: destination file
# TARGET_TO_DEPEND: The copying of the file will be added as
#                   a dependency to this target
function(copy_to_builddir SOURCE_PATH DESTINATION_PATH TARGET_TO_DEPEND)
    ADD_CUSTOM_COMMAND(
        OUTPUT "${DESTINATION_PATH}"
        COMMAND ${CMAKE_COMMAND} -E copy "${SOURCE_PATH}" "${DESTINATION_PATH}"
        DEPENDS "${SOURCE_PATH}"
        COMMENT "Copying \"${SOURCE_PATH}\" into build directory"
    )
    # Add a random number to the end of the string to avoid problems with
    # duplicate filenames
    set(FILENAME "")
    get_filename_component(FILENAME ${SOURCE_PATH} NAME )
    string(RANDOM LENGTH 4 ALPHABET 0123456789 RANDOMNUMBER)
    ADD_CUSTOM_TARGET(copy_${FILENAME}_${RANDOMNUMBER}
        DEPENDS "${DESTINATION_PATH}"
    )
    ADD_DEPENDENCIES(${TARGET_TO_DEPEND} copy_${FILENAME}_${RANDOMNUMBER})
endfunction()

##############################################################################
# configuration-test
##############################################################################

set(TEST_CONFIGURATION configuration-test)

add_executable(${TEST_CONFIGURATION} configuration-test.cpp)
target_link_libraries(${TEST_CONFIGURATION}
    vsomeip
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
)

# The following will make sure that ${TEST_CONFIGURATION_CONFIG_FILE} is copied
# from the config folder into the test folder in the builddirectory
# This makes it possible to call the configuration test within the build directory
set(TEST_CONFIGURATION_CONFIG_FILE vsomeip-test.json)
copy_to_builddir(${PROJECT_SOURCE_DIR}/config/${TEST_CONFIGURATION_CONFIG_FILE}
    ${PROJECT_BINARY_DIR}/test/${TEST_CONFIGURATION_CONFIG_FILE}
    ${TEST_CONFIGURATION}
)

##############################################################################
# magic-cookies-test-client
##############################################################################
set(TEST_MAGIC_COOKIES_NAME magic_cookies_test)

set(TEST_MAGIC_COOKIES_CLIENT magic-cookies-test-client)
add_executable(${TEST_MAGIC_COOKIES_CLIENT} magic-cookies-test-client.cpp)
target_link_libraries(${TEST_MAGIC_COOKIES_CLIENT}
    vsomeip
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
)
add_dependencies(${TEST_MAGIC_COOKIES_CLIENT} response-sample)

# Copy config file for client into $BUILDDIR/test
set(TEST_MAGIC_COOKIES_CLIENT_CONFIG_FILE vsomeip-magic-cookies-client.json)
copy_to_builddir(${PROJECT_SOURCE_DIR}/config/${TEST_MAGIC_COOKIES_CLIENT_CONFIG_FILE}
    ${PROJECT_BINARY_DIR}/test/${TEST_MAGIC_COOKIES_CLIENT_CONFIG_FILE}
    ${TEST_MAGIC_COOKIES_CLIENT}
)

# Copy bashscript to start client into $BUILDDIR/test
set(TEST_MAGIC_COOKIES_CLIENT_UNDERSCORE magic_cookies_test_client)
set(TEST_MAGIC_COOKIES_CLIENT_START_SCRIPT ${TEST_MAGIC_COOKIES_CLIENT_UNDERSCORE}_start.sh)
copy_to_builddir(${PROJECT_SOURCE_DIR}/test/${TEST_MAGIC_COOKIES_CLIENT_START_SCRIPT}
    ${PROJECT_BINARY_DIR}/test/${TEST_MAGIC_COOKIES_CLIENT_START_SCRIPT}
    ${TEST_MAGIC_COOKIES_CLIENT}
)

set(TEST_MAGIC_COOKIES_SERVICE magic_cookies_test_service)

# Copy config file for service into $BUILDDIR/test
set(TEST_MAGIC_COOKIES_SERVICE_CONFIG_FILE vsomeip-magic-cookies-service.json)
copy_to_builddir(${PROJECT_SOURCE_DIR}/config/${TEST_MAGIC_COOKIES_SERVICE_CONFIG_FILE}
    ${PROJECT_BINARY_DIR}/test/${TEST_MAGIC_COOKIES_SERVICE_CONFIG_FILE}
    ${TEST_MAGIC_COOKIES_CLIENT}
)

# Copy bashscript to start service into $BUILDDIR/test
set(TEST_MAGIC_COOKIES_SERVICE_START_SCRIPT ${TEST_MAGIC_COOKIES_SERVICE}_start.sh)
copy_to_builddir(${PROJECT_SOURCE_DIR}/test/${TEST_MAGIC_COOKIES_SERVICE_START_SCRIPT}
    ${PROJECT_BINARY_DIR}/test/${TEST_MAGIC_COOKIES_SERVICE_START_SCRIPT}
    ${TEST_MAGIC_COOKIES_CLIENT}
)

# Copy bashscript to start client and server $BUILDDIR/test
set(TEST_MAGIC_COOKIES_STARTER ${TEST_MAGIC_COOKIES_NAME}_starter.sh)
copy_to_builddir(${PROJECT_SOURCE_DIR}/test/${TEST_MAGIC_COOKIES_STARTER}
    ${PROJECT_BINARY_DIR}/test/${TEST_MAGIC_COOKIES_STARTER}
    ${TEST_MAGIC_COOKIES_CLIENT}
)

##############################################################################
# someip-header-factory-test
##############################################################################
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

##############################################################################
# routing-test
##############################################################################
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

##############################################################################
# payload-test
##############################################################################

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
copy_to_builddir(
    ${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_CONFIG_FILE}
    ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_CONFIG_FILE}
    ${TEST_PAYLOAD_CLIENT}
)

# Copy bashscript to start client into $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_START_SCRIPT ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL}_start.sh)
copy_to_builddir(
    ${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_START_SCRIPT}
    ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_START_SCRIPT}
    ${TEST_PAYLOAD_CLIENT}
)

set(TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CLIENT_EXTERNAL external_local_payload_test_service_client_external)

# Copy bashscript to start service into $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CLIENTT_EXTERNAL_START_SCRIPT ${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CLIENT_EXTERNAL}_start.sh)
copy_to_builddir(
    ${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CLIENTT_EXTERNAL_START_SCRIPT}
    ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_SERVICE_CLIENTT_EXTERNAL_START_SCRIPT}
    ${TEST_PAYLOAD_SERVICE}
)

# Copy bashscript to start client and server $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_STARTER ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_NAME}_starter.sh)
copy_to_builddir(${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_STARTER}
    ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_STARTER}
    ${TEST_PAYLOAD_CLIENT}
)

##############################################################################

# Copy bashscript to start client and server $BUILDDIR/test
set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_NAME external_local_payload_test_client_local_and_external)

set(TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_STARTER ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_NAME}_starter.sh)
copy_to_builddir(${PROJECT_SOURCE_DIR}/test/payload_tests/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_STARTER}
    ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_STARTER}
    ${TEST_PAYLOAD_CLIENT}
)

##############################################################################
# Add for every test a dependency to gtest
##############################################################################

add_dependencies(${TEST_CONFIGURATION} gtest)
add_dependencies(${TEST_MAGIC_COOKIES_CLIENT} gtest)
add_dependencies(${TEST_HEADER_FACTORY} gtest)
add_dependencies(${TEST_HEADER_FACTORY_CLIENT} gtest)
add_dependencies(${TEST_HEADER_FACTORY_SERVICE} gtest)
add_dependencies(${TEST_LOCAL_ROUTING_SERVICE} gtest)
add_dependencies(${TEST_LOCAL_ROUTING_CLIENT} gtest)
add_dependencies(${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE} gtest)
add_dependencies(${TEST_PAYLOAD_SERVICE} gtest)
add_dependencies(${TEST_PAYLOAD_CLIENT} gtest)

##############################################################################
# Add tests to the target build_tests
##############################################################################

add_dependencies(build_tests ${TEST_CONFIGURATION})
add_dependencies(build_tests ${TEST_MAGIC_COOKIES_CLIENT})
add_dependencies(build_tests ${TEST_HEADER_FACTORY})
add_dependencies(build_tests ${TEST_HEADER_FACTORY_CLIENT})
add_dependencies(build_tests ${TEST_HEADER_FACTORY_SERVICE})
add_dependencies(build_tests ${TEST_LOCAL_ROUTING_SERVICE})
add_dependencies(build_tests ${TEST_LOCAL_ROUTING_CLIENT})
add_dependencies(build_tests ${TEST_EXTERNAL_LOCAL_ROUTING_SERVICE})
add_dependencies(build_tests ${TEST_PAYLOAD_SERVICE})
add_dependencies(build_tests ${TEST_PAYLOAD_CLIENT})

##############################################################################
# Add tests
##############################################################################

add_test(NAME ${TEST_CONFIGURATION}
    COMMAND ${TEST_CONFIGURATION} --someip ${TEST_CONFIGURATION_CONFIG_FILE}
)

add_test(NAME ${TEST_MAGIC_COOKIES_NAME}
    COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_MAGIC_COOKIES_STARTER}
)
set_tests_properties(${TEST_MAGIC_COOKIES_NAME} PROPERTIES TIMEOUT 60)

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
set_tests_properties(${TEST_EXTERNAL_LOCAL_ROUTING_NAME} PROPERTIES TIMEOUT 60)

# Payload tests
add_test(NAME ${TEST_LOCAL_PAYLOAD_NAME}
    COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_LOCAL_PAYLOAD_STARTER}
)
add_test(NAME ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_NAME}
    COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_STARTER}
)
add_test(NAME ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_NAME}
    COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_STARTER}
)
set_tests_properties(${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_NAME} PROPERTIES TIMEOUT 240)
add_test(NAME ${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_NAME}
    COMMAND ${PROJECT_BINARY_DIR}/test/${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_LOCAL_AND_EXTERNAL_STARTER}
)
set_tests_properties(${TEST_EXTERNAL_LOCAL_PAYLOAD_CLIENT_EXTERNAL_NAME} PROPERTIES TIMEOUT 240)
