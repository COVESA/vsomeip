// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstdint>
#include <functional>
#include <tuple>

#include <vsomeip/constants.hpp>
#include <vsomeip/primitive_types.hpp>

#include "enumeration_types.hpp"

namespace vsomeip_v3::trace {

using match_t = std::tuple<service_t, instance_t, method_t>;
using filter_func_t = std::function<bool(service_t, instance_t, method_t)>;
using filter_id_t = uint32_t;

// A non-negative filter: its predicate plus the filter type that decides both
// the logging verbosity and whether it restricts the channel to an allow-list
// (only POSITIVE does; HEADER_ONLY and FULL_PAYLOAD do not).
struct trace_filter_entry {
    filter_func_t func;
    filter_type_e type;
};

} // namespace vsomeip_v3::trace
