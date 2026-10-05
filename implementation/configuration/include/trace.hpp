// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <vsomeip/primitive_types.hpp>

#include "../../tracing/include/defines.hpp"
#include "../../tracing/include/enumeration_types.hpp"
#include "../../tracing/include/types.hpp"

namespace vsomeip_v3 {
namespace cfg {

struct trace_channel {
    trace_channel_t id_;
    std::string name_;
};

struct trace_filter {
    trace_filter() : ftype_(vsomeip_v3::trace::filter_type_e::POSITIVE), is_range_(false) { }

    std::vector<trace_channel_t> channels_;
    vsomeip_v3::trace::filter_type_e ftype_;
    bool is_range_;
    std::vector<vsomeip_v3::trace::match_t> matches_;
};

struct trace {
    trace() :
        is_enabled_(false), is_sd_enabled_(false), full_logging_threshold_(VSOMEIP_TC_DEFAULT_FULL_LOGGING_THRESHOLD), channels_(),
        filters_() { }

    bool is_enabled_;
    bool is_sd_enabled_;

    // Full-logging size threshold in bytes; see VSOMEIP_TC_DEFAULT_FULL_LOGGING_THRESHOLD.
    uint32_t full_logging_threshold_;

    std::vector<std::shared_ptr<trace_channel>> channels_;
    std::vector<std::shared_ptr<trace_filter>> filters_;
};

} // namespace cfg
} // namespace vsomeip_v3
