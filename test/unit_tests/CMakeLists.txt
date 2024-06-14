# Copyright (C) 2015-2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project ("unit_tests_bin" LANGUAGES CXX)

file (GLOB SRCS main.cpp **/*.cpp)

set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)
find_package(Boost 1.55 COMPONENTS filesystem system REQUIRED)

# ----------------------------------------------------------------------------
# Executable and libraries to link
# ----------------------------------------------------------------------------
add_executable (${PROJECT_NAME} ${SRCS} )
target_link_libraries (
    ${PROJECT_NAME}
    vsomeip3
    vsomeip3-cfg
    Threads::Threads
    ${Boost_LIBRARIES}
    ${DL_LIBRARY}
    gtest
    vsomeip_utilities
)
enable_testing()
add_test(NAME ${PROJECT_NAME} COMMAND $<TARGET_FILE:${PROJECT_NAME}> WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_dependencies(build_unit_tests ${PROJECT_NAME})
