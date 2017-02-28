// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_CFG_EVENT_HPP
#define VSOMEIP_CFG_EVENT_HPP

#include <memory>
#include <vector>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {
namespace cfg {

struct eventgroup;

struct event {
    event(event_t _id, bool _is_field, bool _is_reliable)
        : id_(_id), is_field_(_is_field), is_reliable_(_is_reliable) {
    }

    event_t id_;
    bool is_field_;
    bool is_reliable_;
    std::vector<std::weak_ptr<eventgroup> > groups_;
};

} // namespace cfg
} // namespace vsomeip

#endif // VSOMEIP_CFG_EVENT_HPP
