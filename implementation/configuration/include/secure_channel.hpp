// Copyright (c) 2017 by Vector Informatik GmbH
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef IMPLEMENTATION_CONFIGURATION_INCLUDE_SECURE_CHANNEL_HPP_
#define IMPLEMENTATION_CONFIGURATION_INCLUDE_SECURE_CHANNEL_HPP_

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include <memory>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {
namespace cfg {

struct secure_channel {
    enum security_level_e {
        PLAIN,
        AUTHENTIC,
        CONFIDENTIAL
    };

    bool is_multicast_;
    secure_channel_t id_;
    std::vector<std::uint8_t> psk_;
    std::string pskid_;
    security_level_e level_;
};

} // namespace cfg
} // namespace vsomeip

#endif  // IMPLEMENTATION_CONFIGURATION_INCLUDE_SECURE_CHANNEL_HPP_
