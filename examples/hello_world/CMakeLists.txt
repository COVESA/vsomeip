# Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG) This Source Code Form is subject to the
# terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with this file, You can obtain
# one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.15)
project(vSomeIPHelloWorld)

find_package(Threads REQUIRED)

include(GNUInstallDirs)

set(INSTALL_BIN_DIR
    ${CMAKE_INSTALL_BINDIR}
    CACHE STRING "Installation directory for executables"
)

add_library(vsomeip_hello_world_example INTERFACE)
target_compile_features(vsomeip_hello_world_example INTERFACE cxx_std_17)
if(ENABLE_SIGNAL_HANDLING)
  target_compile_definitions(vsomeip_hello_world_example INTERFACE VSOMEIP_ENABLE_SIGNAL_HANDLING)
endif()

add_library(vsomeip_hello_world_service INTERFACE)
target_sources(vsomeip_hello_world_service INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/hello_world_service.hpp")
target_link_libraries(vsomeip_hello_world_service INTERFACE vsomeip_hello_world_example)

add_library(vsomeip_hello_world_client INTERFACE)
target_sources(vsomeip_hello_world_client INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/hello_world_client.hpp")
target_link_libraries(vsomeip_hello_world_client INTERFACE vsomeip_hello_world_example)

target_include_directories(vsomeip_hello_world_client INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

if(NOT
   ${CMAKE_SYSTEM_NAME}
   MATCHES
   "Android"
)
  # This will get us acces to the vsomeip target, along with VSOMEIP_INCLUDE_DIRS - include directories for vSomeIP
  # VSOMEIP_LIBRARIES    - libraries to link against
  if(NOT TARGET vsomeip3)
    find_package(vsomeip3)
    if(NOT vsomeip3_FOUND)
      message("vsomeip was not found. Please specify vsomeip_DIR")
    endif()
  endif()

  add_executable(hello_world_service hello_world_service_main.cpp)
  target_link_libraries(hello_world_service PUBLIC vsomeip_hello_world_service vsomeip3 Threads::Threads)

  add_executable(hello_world_client hello_world_client_main.cpp)
  target_link_libraries(hello_world_client PUBLIC vsomeip_hello_world_client vsomeip3 Threads::Threads)

  install(
    TARGETS hello_world_client hello_world_service RUNTIME
            DESTINATION "${INSTALL_BIN_DIR}"
            COMPONENT example-hello_world
  )
endif()
