//
// eventgroup_service_state_machine.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip_internal/sd/eventgroup_service_state_machine.hpp>
#include <vsomeip_internal/sd/service_discovery.hpp>

namespace vsomeip {
namespace sd {
namespace service {

eventgroup_machine::eventgroup_machine(machine *_owner)
	: owner_(_owner),
	  timer_service(_owner->discovery_->get_service()) {
}

void eventgroup_machine::timer_expired(const boost::system::error_code &_error) {

}

not_subscribed::not_subscribed(my_context ctx)
	: sc::state< not_subscribed, eventgroup_machine >(ctx) {
	outermost_context().owner_->log("Ready.NotSubscribed");
}

sc::result not_subscribed::react(const ev_subscribe_eventgroup &_event) {
	outermost_context().owner_->add_subscription(_event.source_, _event.id_);
	return transit< subscribed >();
}

subscribed::subscribed(my_context ctx)
	: sc::state< subscribed, eventgroup_machine >(ctx) {
	outermost_context().owner_->log("Ready.Subscribed");
}

sc::result subscribed::react(const ev_subscribe_eventgroup &_event) {
	outermost_context().owner_->add_subscription(_event.source_, _event.id_);
	return transit< subscribed >();
}

sc::result subscribed::react(const ev_unsubscribe_eventgroup &_event) {
	return transit< not_subscribed >();
}

sc::result subscribed::react(const ev_timeout_expired &_event) {
	return transit< not_subscribed >();
}

} // service
} // namespace sd
} // namespace vsomeip

