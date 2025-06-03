# Copyright (C) 2015-2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG) This Source Code Form is subject to the
# terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with this file, You can obtain
# one at http://mozilla.org/MPL/2.0/.

project("benchmark_tests_bin" LANGUAGES CXX)

file(
  GLOB
  SRCS
  main.cpp
  **/*.cpp
  ../common/utility.cpp
)

set(THREADS_PREFER_PTHREAD_FLAG ON)


# ----------------------------------------------------------------------------
# Executable and libraries to link
# ----------------------------------------------------------------------------
add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE ${SRCS})
target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE ${VSOMEIP_NAME}
          ${VSOMEIP_NAME}-cfg
          vsomeip_utilities
          Threads::Threads
          Boost::filesystem
          Boost::system
          benchmark::benchmark
          gtest
          DL_INTERFACE
)
set_target_properties(vsomeip3-benchmark-tests PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

add_dependencies(build_benchmark_tests ${PROJECT_NAME})
