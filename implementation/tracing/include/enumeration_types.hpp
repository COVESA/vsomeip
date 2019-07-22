// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_TRACE_ENUMERATION_TYPES_HPP
#define VSOMEIP_TRACE_ENUMERATION_TYPES_HPP

namespace vsomeip {
namespace trace {

enum class filter_type_e : uint8_t {
    NEGATIVE = 0x00,
    POSITIVE = 0x01
};

} // namespace trace
} // namespace vsomeip

#endif // VSOMEIP_TC_ENUMERATION_TYPES_HPP
