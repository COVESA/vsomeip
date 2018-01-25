// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONFIGURATION_INCLUDE_TRACE_HPP_
#define CONFIGURATION_INCLUDE_TRACE_HPP_

#include <string>
#include <vector>

#include <vsomeip/primitive_types.hpp>

#include "../../tracing/include/defines.hpp"

namespace vsomeip {
namespace cfg {

struct trace_channel {

    trace_channel() :
        id_(VSOMEIP_TC_DEFAULT_CHANNEL_ID),
        name_(VSOMEIP_TC_DEFAULT_CHANNEL_NAME) {

    }

    trace_channel_t id_;
    std::string name_;
};

struct trace_filter_rule {

    trace_filter_rule() :
        channel_(VSOMEIP_TC_DEFAULT_CHANNEL_ID),
        services_(),
        methods_(),
        clients_(),
        type_(VSOMEIP_TC_DEFAULT_FILTER_TYPE) {

    }

    trace_channel_t channel_;
    std::vector<service_t> services_;
    std::vector<method_t> methods_;
    std::vector<client_t> clients_;
    trace_filter_type_t type_;
};

struct trace {

    trace() :
        channels_(),
        filter_rules_(),
        is_enabled_(false),
        is_sd_enabled_(false) {
        channels_.push_back(std::make_shared<trace_channel>());
    }

    std::vector<std::shared_ptr<trace_channel>> channels_;
    std::vector<std::shared_ptr<trace_filter_rule>> filter_rules_;

    bool is_enabled_;
    bool is_sd_enabled_;
};

} // namespace cfg
} // namespace vsomeip

#endif /* CONFIGURATION_INCLUDE_TRACE_HPP_ */
