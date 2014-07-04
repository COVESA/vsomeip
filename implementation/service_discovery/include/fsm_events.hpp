// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_FSM_EVENTS_HPP
#define VSOMEIP_FSM_EVENTS_HPP

#include <boost/statechart/event.hpp>

#include <vsomeip/primitive_types.hpp>

namespace sc = boost::statechart;

namespace vsomeip {
namespace sd {

struct ev_none: sc::event< ev_none > {};
struct ev_timeout: sc::event< ev_timeout > {};

struct ev_status_change: sc::event< ev_status_change > {
	ev_status_change(bool _is_up) : is_up_(_is_up) {};

	bool is_up_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_FSM_EVENTS_HPP
