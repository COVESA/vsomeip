// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_CFG_SERVICE_HPP
#define VSOMEIP_CFG_SERVICE_HPP

#include <memory>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {
namespace cfg {

struct event;
struct eventgroup;

struct service {
    service_t service_;
    instance_t instance_;

    std::string unicast_address_;

    uint16_t reliable_;
    uint16_t unreliable_;

    std::string multicast_address_;
    uint16_t multicast_port_;

    std::string protocol_;

    std::shared_ptr<servicegroup> group_;
    std::map<event_t, std::shared_ptr<event> > events_;
    std::map<eventgroup_t, std::shared_ptr<eventgroup> > eventgroups_;
};

} // namespace cfg
} // namespace vsomeip

#endif // VSOMEIP_CFG_SERVICE_HPP
