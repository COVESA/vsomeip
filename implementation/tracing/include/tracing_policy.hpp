// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstdint>

#include "enumeration_types.hpp"

namespace vsomeip_v3 {
namespace trace {

// Decide whether a matched message is logged with its full payload or header
// only. An explicit positive filter always logs the full payload (bypasses the
// threshold); the forward-everything default logs the full payload only up to
// the size threshold (0 == no threshold, i.e. always full). DROP is expected to
// be handled by the caller before this is reached.
inline bool should_log_full(trace_result_e _result, uint32_t _data_size, uint32_t _threshold) {
    switch (_result) {
    case trace_result_e::POSITIVE_FILTER:
        return true;
    case trace_result_e::DEFAULT:
        return _threshold == 0 || _data_size <= _threshold;
    default: // HEADER_ONLY_FILTER, DROP
        return false;
    }
}

} // namespace trace
} // namespace vsomeip_v3
