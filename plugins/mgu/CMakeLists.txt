# Copyright (C) 2016-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required (VERSION 2.8)

set (VSOMEIP_DIAGNOSIS_PLUGIN_MAJOR_VERSION 1)
set (VSOMEIP_DIAGNOSIS_PLUGIN_MINOR_VERSION 0)
set (VSOMEIP_DIAGNOSIS_PLUGIN_PATCH_VERSION 0)

if(DEFINED VSOMEIP_VERSION)
    set (VSOMEIP_DIAGNOSIS_PLUGIN_VERSION ${VSOMEIP_VERSION}-${VSOMEIP_DIAGNOSIS_PLUGIN_MAJOR_VERSION}.${VSOMEIP_DIAGNOSIS_PLUGIN_MINOR_VERSION}.${VSOMEIP_DIAGNOSIS_PLUGIN_PATCH_VERSION})
else()
    find_package (vsomeip 2.7.0 REQUIRED)
    set (VSOMEIP_DIAGNOSIS_PLUGIN_VERSION ${vsomeip_VERSION}-${VSOMEIP_DIAGNOSIS_PLUGIN_MAJOR_VERSION}.${VSOMEIP_DIAGNOSIS_PLUGIN_MINOR_VERSION}.${VSOMEIP_DIAGNOSIS_PLUGIN_PATCH_VERSION})
endif()
message(STATUS "vsomeip-diagnosis-plugin-mgu: " ${VSOMEIP_DIAGNOSIS_PLUGIN_VERSION})

# Offer the user the choice of overriding the installation directories
set (INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set (INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
set (INSTALL_INCLUDE_DIR include CACHE PATH "Installation directory for header files")

include_directories (
    include
    ${VSOMEIP_INCLUDE_DIRS}
)

# OS
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(OS "LINUX")
    set(DL_LIBRARY "dl")
    set(NO_DEPRECATED "")
    set(OPTIMIZE "")
    set(OS_CXX_FLAGS "-D_GLIBCXX_USE_NANOSLEEP -pthread -O -Wall -Wextra -Wformat -Wformat-security -Wconversion -fexceptions -fstrict-aliasing -fstack-protector -fasynchronous-unwind-tables -fno-omit-frame-pointer -D_FORTIFY_SOURCE=2")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D${OS} ${OS_CXX_FLAGS} -g ${OPTIMIZE} -std=c++0x ${NO_DEPRECATED}")
set(USE_RT "rt")

# MGU diagnosis job plugin library
file(GLOB vsomeip-diagnosis-plugin-mgu_SRC
    "src/diagnosis_plugin_mgu.cpp"
)
list(SORT vsomeip-diagnosis-plugin-mgu_SRC)

add_library(vsomeip-diagnosis-plugin-mgu SHARED ${vsomeip-diagnosis-plugin-mgu_SRC})
set_target_properties (vsomeip-diagnosis-plugin-mgu PROPERTIES VERSION ${VSOMEIP_DIAGNOSIS_PLUGIN_VERSION} SOVERSION ${VSOMEIP_DIAGNOSIS_PLUGIN_MAJOR_VERSION})
target_link_libraries(vsomeip-diagnosis-plugin-mgu vsomeip)

install (
    TARGETS vsomeip-diagnosis-plugin-mgu
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
)

###################################################################################################