# Copyright (C) 2024 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.4...3.22)

set(TEST_NAME offer_stop_offer_test)

macro(add_test_application application_sufix)

    add_executable(
        ${TEST_NAME}_${application_sufix}
        ${TEST_NAME}_${application_sufix}.cpp
        applications/${application_sufix}.cpp
    )
    target_link_libraries(
        ${TEST_NAME}_${application_sufix}
        ${VSOMEIP_NAME}
        ${Boost_LIBRARIES}
        ${DL_LIBRARY}
        ${TEST_LINK_LIBRARIES}
        vsomeip_utilities
    )
    add_dependencies(${TEST_NAME}_${application_sufix} gtest)
    add_dependencies(build_network_tests ${TEST_NAME}_${application_sufix})
    configure_file(
        ${NETWORK_TEST_SRC_DIR}/${TEST_NAME}/conf/${TEST_NAME}_${application_sufix}.json.in
        ${NETWORK_TEST_BIN_DIR}/${TEST_NAME}/${TEST_NAME}_${application_sufix}.json
        @ONLY)
    copy_to_builddir(
        ${NETWORK_TEST_SRC_DIR}/${TEST_NAME}/${TEST_NAME}_${application_sufix}_starter.sh
        ${NETWORK_TEST_BIN_DIR}/${TEST_NAME}/${TEST_NAME}_${application_sufix}_starter.sh
        ${TEST_NAME}_${application_sufix}
    )

endmacro()

add_test_application(service)
add_test_application(client)

add_test(
    NAME ${TEST_NAME}
    COMMAND ${NETWORK_TEST_BIN_DIR}/${TEST_NAME}/${TEST_NAME}_client_starter.sh
)
set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT 30)
