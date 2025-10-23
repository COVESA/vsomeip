# Copyright (C) 2015-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at http://mozilla.org/MPL/2.0/.

project(unit_tests_usei_tests LANGUAGES CXX)

set(THREADS_PREFER_PTHREAD_FLAG ON)

file(GLOB SRCS *.cpp ../main.cpp
     "../../../implementation/endpoints/src/udp_server_endpoint_impl.cpp")
add_executable(${PROJECT_NAME} ${SRCS})

target_link_libraries(
  ${PROJECT_NAME}
  vsomeip3-cfg
  Threads::Threads
  ${Boost_LIBRARIES}
  ${DL_LIBRARY}
  gtest
  gmock
  vsomeip_utilities
  "-Xlinker --wrap=setsockopt")

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

add_dependencies(build_unit_tests ${PROJECT_NAME})
add_dependencies(build_network_tests ${PROJECT_NAME})
