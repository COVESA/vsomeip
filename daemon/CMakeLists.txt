# Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_custom_target(daemon)

add_executable(vsomeipd vsomeipd.cpp)
target_link_libraries(vsomeipd vsomeip::vsomeip)

install(TARGETS vsomeipd
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT bin
    )

target_compile_definitions(vsomeipd PUBLIC ${OS_COMPILER_DEFINITIONS})
target_compile_options(vsomeipd PUBLIC ${OS_COMPILER_FLAGS})

set_target_properties(vsomeipd PROPERTIES CXX_STANDARD 14)
