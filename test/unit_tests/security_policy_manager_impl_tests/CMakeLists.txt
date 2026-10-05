# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project("unit_tests_policy_manager_impl_tests" LANGUAGES CXX)

file(GLOB SRCS ../main.cpp *.cpp)

set(THREADS_PREFER_PTHREAD_FLAG ON)

# ----------------------------------------------------------------------------
# Executable and libraries to link
# ----------------------------------------------------------------------------
add_executable(${PROJECT_NAME} ${SRCS})

target_compile_definitions(${PROJECT_NAME}
    PRIVATE UNIT_TEST_BUILD_DIR_PATH="${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(
    ${PROJECT_NAME}
    vsomeip3-test
    vsomeip3-cfg-test
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    gtest
    vsomeip_utilities
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/security_policy_manager_impl_tests/0_0)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

add_dependencies(build_unit_tests ${PROJECT_NAME})
