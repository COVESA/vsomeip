# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

project(configuration_tests LANGUAGES CXX)

set(TEST_CONFIGURATION configuration_test)

add_executable(${TEST_CONFIGURATION}
    ${TEST_CONFIGURATION}.cpp
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
set(TEST_CONFIGURATION_CONFIG_FILE configuration_test.json)
set(TEST_CONFIGURATION_DEPRECATED_CONFIG_FILE configuration_test_deprecated.json)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/configuration_tests/${TEST_CONFIGURATION_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/configuration_tests/${TEST_CONFIGURATION_CONFIG_FILE}
    ${TEST_CONFIGURATION}
)
copy_to_builddir(${NETWORK_TEST_SRC_DIR}/configuration_tests/${TEST_CONFIGURATION_DEPRECATED_CONFIG_FILE}
    ${NETWORK_TEST_BIN_DIR}/configuration_tests/${TEST_CONFIGURATION_DEPRECATED_CONFIG_FILE}
    ${TEST_CONFIGURATION}
)

string(REPLACE "test_name" "${TEST_CONFIGURATION}" TEST_CONFIGURATION_ENTRYPOINT "${TEST_ENTRYPOINT}")
add_test(NAME ${TEST_CONFIGURATION}
    COMMAND ${TEST_CONFIGURATION_ENTRYPOINT} ${NETWORK_TEST_BIN_DIR}/configuration_tests/${TEST_CONFIGURATION}
)

add_dependencies(${TEST_CONFIGURATION} gtest)

add_dependencies(build_network_tests ${TEST_CONFIGURATION})
