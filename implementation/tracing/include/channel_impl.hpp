// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <string>
#include <vector>

#include <vsomeip/constants.hpp>
#include <vsomeip/primitive_types.hpp>

#include "enumeration_types.hpp"
#include "types.hpp"

#ifdef USE_DLT
#include <dlt/dlt.h>
#else
struct DltContext;
#endif

namespace vsomeip_v3::trace {

// Note: Nothing in this class is thread-safe.
// Instances are expected to be created and configured from connector_impl's ctor only.
class channel_impl {
public:
    channel_impl(const std::string& _id, DltContext* _log_context);

    std::string get_id() const;
    DltContext* get_log_context() const;

    void add_filter(const match_t& _match, filter_type_e _type);
    void add_filter(const std::vector<match_t>& _matches, filter_type_e _type);
    void add_filter(const match_t& _from, const match_t& _to, filter_type_e _type);

    trace_result_e matches(service_t _service, instance_t _instance, method_t _method);

private:
    void add_filter_intern(filter_func_t _func, filter_type_e _type);

    std::string id_;
    DltContext* log_context_;

    std::vector<trace_filter_entry> positive_; // POSITIVE, HEADER_ONLY, FULL_PAYLOAD
    std::vector<filter_func_t> negative_;
};

} // namespace vsomeip_v3::trace
