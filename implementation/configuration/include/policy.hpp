// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_CFG_POLICY_HPP
#define VSOMEIP_CFG_POLICY_HPP

#include <memory>
#include <set>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {
namespace cfg {

typedef std::set<std::pair<uint32_t, uint32_t>> ranges_t;
typedef std::set<std::pair<ranges_t, ranges_t>> ids_t;

struct policy {
    policy() : allow_who_(false), allow_what_(false) {};

    ids_t ids_;
    bool allow_who_;

    std::set<std::pair<service_t, instance_t>> services_;
    std::set<std::pair<service_t, instance_t>> offers_;
    bool allow_what_;
};

} // namespace cfg
} // namespace vsomeip

#endif // VSOMEIP_CFG_POLICY_HPP
