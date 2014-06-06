//
// eventgroup_client_state_machine.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <iomanip>

#include <vsomeip_internal/sd/client_state_machine.hpp>
#include <vsomeip_internal/sd/eventgroup_client_state_machine.hpp>
#include <vsomeip_internal/sd/service_discovery.hpp>

namespace vsomeip {
namespace sd {
namespace client {

eventgroup_machine::eventgroup_machine(machine *_owner)
	: timer_service(_owner->discovery_->get_service()),
	  owner_(_owner) {
}

void eventgroup_machine::timer_expired(const boost::system::error_code &_error) {
}

// State "not_subscribed"
not_subscribed::not_subscribed(my_context ctx)
	: sc::state< not_subscribed, eventgroup_machine >(ctx) {
	outermost_context().owner_->log("NotSubscribed");
	outermost_context().owner_->send_subscribe();
}

sc::result not_subscribed::react(const ev_timeout_expired &_event) {
	return discard_event();
}

sc::result not_subscribed::react(const ev_subscribe_eventgroup_ack &_event) {
	return transit< subscribed >();
}

sc::result not_subscribed::react(const ev_request_eventgroup &_event) {
	return discard_event();
}

sc::result not_subscribed::react(const ev_release_eventgroup &_event) {
	return discard_event();
}

subscribed::subscribed(my_context ctx)
	: sc::state< subscribed, eventgroup_machine >(ctx) {
	outermost_context().owner_->log("Seen.Subscribed");
}

sc::result subscribed::react(const ev_request_eventgroup &_event) {
	return discard_event();
}

sc::result subscribed::react(const ev_release_eventgroup &_event) {
	return discard_event();
}

} // namespace client
} // namespace sd
} // namespace vsomeip
