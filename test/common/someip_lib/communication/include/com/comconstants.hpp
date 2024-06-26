// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef __COMCONSTANTS_HPP__
#define __COMCONSTANTS_HPP__

#include <string>

namespace vsomeip_utilities {
namespace com {

/**
 * @brief Message terminator string
 *
 */
constexpr std::string_view MSG_TERMINATOR = { "\n" };

/**
 * @brief Default message buffer size
 *
 */
constexpr int DEFAULT_BUFFER_SIZE = 4096;

} // com
} // vsomeip_utilities

#endif // __COMCONSTANTS_HPP__
