# Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required (VERSION 3.13)
project (vSomeIPHelloWorld)

include(GNUInstallDirs)

find_package(Threads REQUIRED)

# This will get us acces to
#   VSOMEIP_INCLUDE_DIRS - include directories for vSomeIP
#   VSOMEIP_LIBRARIES    - libraries to link against
find_package(vsomeip3 REQUIRED)

# create_target("executable")
function(create_target executable)
    add_library(vsomeip_hello_world_${executable} INTERFACE)
    target_sources(vsomeip_hello_world_${executable} INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/hello_world_${executable}.hpp"
    )
    target_compile_features(vsomeip_hello_world_${executable} INTERFACE cxx_std_17)

    target_include_directories(vsomeip_hello_world_${executable} INTERFACE
        ${PROJECT_SOURCE_DIR}
    )
endfunction()

# link_target("executable")
function(link_target executable)
    add_executable(hello_world_${executable})
    target_sources(hello_world_${executable} PRIVATE hello_world_${executable}_main.cpp)
    target_link_libraries(hello_world_${executable} PRIVATE vsomeip_hello_world_${executable} vsomeip3 Threads::Threads)
    if(ENABLE_SIGNAL_HANDLING)
      target_compile_definitions(vsomeip_hello_world_${executable} PRIVATE VSOMEIP_ENABLE_SIGNAL_HANDLING)
    endif()
endfunction()

create_target("service")
create_target("client")

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Android")

    link_target("client")
    link_target("service")

    install(
        TARGETS
            hello_world_client hello_world_service RUNTIME
         DESTINATION "${INSTALL_BIN_DIR}"
         COMPONENT example-hello_world
    )
endif()
