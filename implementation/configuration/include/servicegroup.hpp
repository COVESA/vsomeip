// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_CFG_SERVICEGROUP_HPP
#define VSOMEIP_CFG_SERVICEGROUP_HPP

namespace vsomeip {
namespace cfg {

struct servicegroup {
    std::string name_; // Name of the ServiceGroup
    std::string unicast_; // either "local" or an IP address

    uint32_t min_initial_delay_;
    uint32_t max_initial_delay_;
    uint32_t repetition_base_delay_;
    uint32_t cyclic_offer_delay_;
    uint32_t cyclic_request_delay_;
    uint8_t repetition_max_;
};

} // namespace cfg
} // namespace vsomeip

#endif // VSOMEIP_CFG_SERVICEGROUP_HPP
