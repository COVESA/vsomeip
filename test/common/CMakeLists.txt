# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project (vsomeip_utilities)

# ----------------------------------------------------------------------------
# Include all source files (cpp/hpp)
# ----------------------------------------------------------------------------
file (GLOB SRC src/*.cpp)
file (GLOB INC include/common/*.hpp, include/common/vsomeip_app_utilities/*.hpp)

# ----------------------------------------------------------------------------
# Declare the library
# ----------------------------------------------------------------------------
add_library (
    vsomeip_utilities
    ${SRC}
    ${INC}
)

target_link_libraries (
    vsomeip_utilities
    PUBLIC
        ${VSOMEIP_NAME}
    PRIVATE
        Boost::system
        Boost::filesystem
    INTERFACE
        DL_INTERFACE
)

# ----------------------------------------------------------------------------
# Specify here the include directories exported
# by this library
# ----------------------------------------------------------------------------
target_include_directories (
    vsomeip_utilities
    PUBLIC include
    PRIVATE src
)

if (MSVC)
	set_target_properties(vsomeip_utilities PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif ()
