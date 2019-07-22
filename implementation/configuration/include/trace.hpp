// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CFG_TRACE_HPP_
#define CFG_TRACE_HPP_

#include <string>
#include <vector>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/trace.hpp>

namespace vsomeip {
namespace cfg {

struct trace_channel {
    trace_channel_t id_;
    std::string name_;
};

struct trace_filter {
    trace_filter()
        : is_positive_(true),
          is_range_(false) {
    }

    std::vector<trace_channel_t> channels_;
    bool is_positive_;
    bool is_range_;
    std::vector<vsomeip::trace::match_t> matches_;
};

struct trace {
    trace()
        : is_enabled_(false),
          is_sd_enabled_(false),
          channels_(),
          filters_() {
    }

    bool is_enabled_;
    bool is_sd_enabled_;

    std::vector<std::shared_ptr<trace_channel>> channels_;
    std::vector<std::shared_ptr<trace_filter>> filters_;
};

} // namespace cfg
} // namespace vsomeip

#endif // CFG_TRACE_HPP_
