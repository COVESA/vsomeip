// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstdint>

namespace vsomeip_v3 {
namespace trace {

enum class filter_type_e : uint8_t { NEGATIVE = 0x00, POSITIVE = 0x01, HEADER_ONLY = 0x02, FULL_PAYLOAD = 0x03 };

// Outcome of matching a message against a channel's filter set. These describe
// *what matched*; the connector maps them to a logging verbosity (see
// should_log_full()).
enum class trace_result_e : uint8_t {
    DROP, // negative filter matched, or positive filters exist but none matched
    POSITIVE_FILTER, // an explicit positive or full-payload filter matched -> log full payload
    HEADER_ONLY_FILTER, // a header-only filter matched
    DEFAULT // no positive filter defined -> forward-everything default
};

} // namespace trace
} // namespace vsomeip_v3
