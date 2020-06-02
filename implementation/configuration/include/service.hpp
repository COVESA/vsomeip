// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_CFG_SERVICE_HPP
#define VSOMEIP_V3_CFG_SERVICE_HPP

#include <memory>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip_v3 {
namespace cfg {

struct event;
struct eventgroup;

struct service {
    service_t service_;
    instance_t instance_;

    std::string unicast_address_;

    uint16_t reliable_;
    uint16_t unreliable_;

    major_version_t major_;
    minor_version_t minor_;
    ttl_t ttl_;

    std::string multicast_address_;
    uint16_t multicast_port_;

    std::string protocol_;

    // [0] = debounce_time
    // [1] = retention_time
    typedef std::map<method_t, std::array<std::chrono::nanoseconds, 2>> npdu_time_configuration_t;
    npdu_time_configuration_t debounce_times_requests_;
    npdu_time_configuration_t debounce_times_responses_;

    std::shared_ptr<servicegroup> group_;
    std::map<event_t, std::shared_ptr<event> > events_;
    std::map<eventgroup_t, std::shared_ptr<eventgroup> > eventgroups_;

    // SOME/IP-TP
    std::set<method_t> tp_segment_messages_client_to_service_;
    std::set<method_t> tp_segment_messages_service_to_client_;
};

} // namespace cfg
} // namespace vsomeip_v3

#endif // VSOMEIP_V3_CFG_SERVICE_HPP
