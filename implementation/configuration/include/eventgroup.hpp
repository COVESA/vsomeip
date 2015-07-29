// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_CFG_EVENTGROUP_HPP
#define VSOMEIP_CFG_EVENTGROUP_HPP

#include <memory>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {
namespace cfg {

struct event;

struct eventgroup {
    eventgroup_t id_;
    std::set<std::shared_ptr<event> > events_;
};

} // namespace cfg
} // namespace vsomeip

#endif // VSOMEIP_CFG_EVENTGROUP_HPP
