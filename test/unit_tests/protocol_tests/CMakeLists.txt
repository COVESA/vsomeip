# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project("unit_tests_protocol_tests" LANGUAGES CXX)

file(GLOB TEST_SRCS main.cpp *.cpp)

add_executable(
    ${PROJECT_NAME}
    ${TEST_SRCS}
)

target_link_libraries(
    ${PROJECT_NAME}
    vsomeip3-test
    vsomeip3-cfg-test
    gtest
)

add_dependencies(build_unit_tests ${PROJECT_NAME})

add_test(
    NAME ${PROJECT_NAME}
    COMMAND ${PROJECT_NAME}
)
