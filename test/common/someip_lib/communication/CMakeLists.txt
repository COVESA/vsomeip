# Copyright (C) 2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project (communication)

# ----------------------------------------------------------------------------
# Include all source files (cpp/hpp)
# ----------------------------------------------------------------------------
file (GLOB SRC src/*.cpp src/*.hpp)
file (GLOB INC include/*.hpp include/com/*.hpp)

# ----------------------------------------------------------------------------
# Declare the library
# ----------------------------------------------------------------------------
add_library (
    ${PROJECT_NAME} SHARED
    ${SRC}
    ${INC}
)

target_link_libraries (
    ${PROJECT_NAME}
    PUBLIC
        ${BOOST_LIBS}
        utils
        serialization
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
