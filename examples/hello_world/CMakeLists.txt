# Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required (VERSION 2.8.7)
project (vSomeIPHelloWorld)

# This will get us acces to
#   VSOMEIP_INCLUDE_DIRS - include directories for vSomeIP
#   VSOMEIP_LIBRARIES    - libraries to link against
find_package(vsomeip)
if (NOT vsomeip_FOUND)
    message("vsomeip was not found. Please specify vsomeip_DIR")
endif()

find_package(Threads REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(${VSOMEIP_INCLUDE_DIRS})

add_executable (hello_world_service hello_world_service.cpp)
target_link_libraries(hello_world_service ${VSOMEIP_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

add_executable (hello_world_client hello_world_client.cpp)
target_link_libraries(hello_world_client ${VSOMEIP_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
