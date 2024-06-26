# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project (someip)

# ----------------------------------------------------------------------------
# Include all source files (cpp/hpp)
# ----------------------------------------------------------------------------
file (GLOB
    SRC
        src/*.cpp src/*.hpp
        src/sd/*.cpp src/sd/*.hpp
        src/sd/options/*.cpp src/sd/options/*.hpp
        src/generic/*.cpp src/generic/*.hpp
)

file (GLOB INC include/*.hpp)

# ----------------------------------------------------------------------------
# Declare the library
# ----------------------------------------------------------------------------
add_library (
    ${PROJECT_NAME} SHARED
    ${SRC}
    ${INC}
)

TARGET_LINK_LIBRARIES (
    ${PROJECT_NAME}
    PUBLIC
        utils
        serialization
        communication
)

# ----------------------------------------------------------------------------
# Specify here the include directories exported
# by this library
# ----------------------------------------------------------------------------
target_include_directories (
    ${PROJECT_NAME}
    PUBLIC include
    PRIVATE src
)
