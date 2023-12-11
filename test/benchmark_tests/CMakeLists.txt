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
add_executable(vsomeip3-benchmark-tests)
target_sources(vsomeip3-benchmark-tests PRIVATE ${SRCS})
target_link_libraries(
  vsomeip3-benchmark-tests
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

add_dependencies(build_benchmark_tests vsomeip3-benchmark-tests)
