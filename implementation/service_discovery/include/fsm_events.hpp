// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_FSM_EVENTS_HPP
#define VSOMEIP_FSM_EVENTS_HPP

#include <boost/statechart/event.hpp>

#include <vsomeip/primitive_types.hpp>
#include "../../routing/include/serviceinfo.hpp"

namespace sc = boost::statechart;

namespace vsomeip {
namespace sd {

struct ev_none: sc::event<ev_none> {
};

struct ev_timeout: sc::event<ev_timeout> {
};

struct ev_alt_timeout: sc::event<ev_alt_timeout> {
};

struct ev_status_change: sc::event<ev_status_change> {
    ev_status_change(bool _is_up)
            : is_up_(_is_up) {
    }

    bool is_up_;
};

struct ev_find_service: sc::event<ev_find_service> {

    ev_find_service(const std::shared_ptr<const serviceinfo> &_info, service_t _service, instance_t _instance,
            major_version_t _major, minor_version_t _minor, bool _unicast_flag)
            : info_(_info), service_(_service), instance_(_instance), major_(_major), minor_(
                    _minor), unicast_flag_(_unicast_flag) {
    }
    const std::shared_ptr<const serviceinfo> &info_;
    service_t service_;
    instance_t instance_;
    major_version_t major_;
    minor_version_t minor_;
    bool unicast_flag_;
};

struct ev_offer_change: sc::event<ev_offer_change> {
};

struct ev_request_service: sc::event<ev_request_service> {
};

}  // namespace sd
}  // namespace vsomeip

#endif // VSOMEIP_FSM_EVENTS_HPP
