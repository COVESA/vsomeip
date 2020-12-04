// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_CFG_EVENT_HPP
#define VSOMEIP_V3_CFG_EVENT_HPP

#include <memory>
#include <vector>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip_v3 {
namespace cfg {

struct eventgroup;

struct event {
    event(event_t _id)
        : id_(_id),
          is_placeholder_(true),
          is_field_(false),
          reliability_(reliability_type_e::RT_UNRELIABLE) {
    }

    event_t id_;
    bool is_placeholder_;
    bool is_field_;
    reliability_type_e reliability_;
    std::vector<std::weak_ptr<eventgroup> > groups_;
};

} // namespace cfg
} // namespace vsomeip_v3

#endif // VSOMEIP_V3_CFG_EVENT_HPP
