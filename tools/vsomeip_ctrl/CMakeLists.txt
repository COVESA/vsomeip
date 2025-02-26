# Copyright (C) 2016-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG) This Source Code Form is subject to the
# terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with this file, You can obtain
# one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.13)

# vsomeip_ctrl
add_executable(vsomeip_ctrl EXCLUDE_FROM_ALL vsomeip_ctrl.cpp)
target_link_libraries(
  vsomeip_ctrl
  PUBLIC ${VSOMEIP_NAME}
  PRIVATE Boost::filesystem DL_INTERFACE Threads::Threads
)

install(
  TARGETS vsomeip_ctrl RUNTIME
          DESTINATION "${INSTALL_BIN_DIR}"
          COMPONENT vsomeip_ctrl
          EXCLUDE_FROM_ALL
)
