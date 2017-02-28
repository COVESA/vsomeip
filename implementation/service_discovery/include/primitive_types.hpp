// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cstdint>

#ifndef VSOMEIP_SD_PRIMITIVE_TYPES_HPP
#define VSOMEIP_SD_PRIMITIVE_TYPES_HPP

namespace vsomeip {
namespace sd {

// Load balancing
typedef uint16_t priority_t;
typedef uint16_t weight_t;

// Protection
typedef uint32_t alive_counter_t;
typedef uint32_t crc_t;

//
typedef uint8_t flags_t;

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_PRIMITIVE_TYPES_HPP
