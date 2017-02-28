// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_CFG_POLICY_HPP
#define VSOMEIP_CFG_POLICY_HPP

#include <memory>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {
namespace cfg {

struct policy {
    std::set<std::pair<service_t, instance_t>> allowed_services_;
    std::set<std::pair<service_t, instance_t>> allowed_offers_;
    std::set<std::pair<service_t, instance_t>> denied_services_;
    std::set<std::pair<service_t, instance_t>> denied_offers_;
    std::uint32_t uid_;
    std::uint32_t gid_;
    bool allow_;
};

} // namespace cfg
} // namespace vsomeip

#endif // VSOMEIP_CFG_POLICY_HPP
