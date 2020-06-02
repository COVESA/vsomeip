// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_POLICY_HPP_
#define VSOMEIP_V3_POLICY_HPP_

#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <utility>

#include <vsomeip/constants.hpp>
#include <vsomeip/primitive_types.hpp>
#include <vsomeip/internal/logger.hpp>

namespace vsomeip_v3 {

typedef std::set<std::pair<uint32_t, uint32_t>> ranges_t;
typedef std::set<std::pair<ranges_t, ranges_t>> ids_t;

struct policy {
    policy() : allow_who_(false), allow_what_(false) {};

    ids_t ids_;
    bool allow_who_;

    std::map<service_t, ids_t> services_;
    std::map<service_t, ranges_t> offers_;
    bool allow_what_;

    std::mutex mutex_;
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_POLICY_HPP_
