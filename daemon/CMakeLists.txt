# Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required (VERSION 2.8)

# Daemon
add_executable(vsomeipd vsomeipd.cpp)
target_link_libraries(vsomeipd vsomeip ${Boost_LIBRARIES} ${DL_LIBRARY} ${DLT_LIBRARIES})

install (
    TARGETS vsomeipd
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
)

###################################################################################################
