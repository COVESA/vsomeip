// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <map>
#include <set>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/plugin.hpp>

#include "debounce_filter_impl.hpp"

namespace vsomeip_v3 {

namespace cfg {

struct application_configuration {
    client_t client_;
    std::size_t max_dispatchers_;
    std::size_t max_dispatch_time_;
    std::size_t thread_count_;
    uint32_t log_status_interval_;
    uint32_t log_version_interval_;
    std::size_t request_debounce_time_;
    std::map<plugin_type_e, std::set<std::string>> plugins_;
    int nice_level_;
    debounce_configuration_t debounces_;
    bool has_session_handling_;
};

} // namespace cfg
} // namespace vsomeip_v3
