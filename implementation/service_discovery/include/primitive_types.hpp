// Copyright (C) 2014-2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cstdint>

#ifndef VSOMEIP_V3_SD_PRIMITIVE_TYPES_HPP_
#define VSOMEIP_V3_SD_PRIMITIVE_TYPES_HPP_

namespace vsomeip_v3 {
namespace sd {

// Load balancing
using priority_t = std::uint16_t;
using weight_t = std::uint16_t;

// Protection
using alive_counter_t = std::uint32_t;
using crc_t = std::uint32_t;

//
using flags_t = std::uint8_t;

} // namespace sd
} // namespace vsomeip_v3

#endif // VSOMEIP_V3_SD_PRIMITIVE_TYPES_HPP_
