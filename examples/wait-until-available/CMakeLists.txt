# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_executable(wait-until-available wait-until-available.cpp)
target_link_libraries(wait-until-available ${VSOMEIP_NAME})
add_dependencies(wait-until-available ${VSOMEIP_NAME})
