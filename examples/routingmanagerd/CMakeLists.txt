# Copyright (C) 2015-2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG) This Source Code Form is subject to the
# terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with this file, You can obtain
# one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.15)


option(VSOMEIP_INSTALL_ROUTINGMANAGERD "Whether or not to install the routing manager daemon.")

# Daemon
add_executable(routingmanagerd)
target_sources(routingmanagerd PRIVATE routingmanagerd.cpp)

target_compile_definitions(routingmanagerd PRIVATE $<TARGET_NAME_IF_EXISTS:USE_DLT>)

target_link_libraries(
  routingmanagerd
  PRIVATE ${VSOMEIP_NAME}
          Boost::system
          Boost::filesystem
          $<TARGET_NAME_IF_EXISTS:PkgConfig::DLT>
          OS_INTERFACE
          DL_INTERFACE
          RT_INTERFACE
          Threads::Threads
)

if(VSOMEIP_INSTALL_ROUTINGMANAGERD)
  install(
    TARGETS routingmanagerd RUNTIME
            DESTINATION "${INSTALL_BIN_DIR}"
            COMPONENT bin
  )
endif()

# ######################################################################################################################
