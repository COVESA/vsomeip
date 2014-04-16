//
// client_behavior.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/application.hpp>
#include <vsomeip_internal/sd/client_manager.hpp>
#include <vsomeip_internal/sd/client_state_machine.hpp>

namespace vsomeip {
namespace sd {
namespace client_state_machine {

machine::machine(client_manager *_manager)
	: timer_service(_manager->get_service()),
	  manager_(_manager) {
}

machine::~machine() {
}

void machine::timer_expired(const boost::system::error_code &_error) {
	if (!_error) {
		process_event(ev_timeout_expired());
	}
}

// State "not_seen"
not_seen::not_seen() {
}

sc::result not_seen::react(const ev_offer_service &_event) {
	return transit< seen >();
}

// State "waiting"
waiting::waiting() {
}

sc::result waiting::react(const ev_none &_event) {
	if (outermost_context().is_ready() && outermost_context().is_service_requested_)
		return transit< initializing >();

	return discard_event();
}

sc::result waiting::react(const ev_service_status_change &_event) {
	outermost_context().is_service_ready_ = _event.is_ready_;

	if (outermost_context().is_ready() && outermost_context().is_service_requested_)
		return transit< initializing >();

	return discard_event();
}

sc::result waiting::react(const ev_request_status_change &_event) {
	outermost_context().is_service_requested_ = _event.is_requested_;

	if (outermost_context().is_ready() && outermost_context().is_service_requested_)
		return transit< initializing >();

	return discard_event();
}

// State "initializing"
initializing::initializing() {
}

sc::result initializing::react(const ev_timeout_expired &_event) {
	return transit< searching >();
}

// State "searching"
searching::searching() {
}

sc::result searching::react(const ev_timeout_expired &_event) {
	return transit< searching >();
}

// State "seen"
seen::seen() {
}

sc::result seen::react(const ev_timeout_expired &_event) {
	return transit< not_seen >();
}

sc::result seen::react(const ev_stop_offer_service &_event) {
	return transit< not_seen >();
}

sc::result seen::react(const ev_service_status_change &_event) {
	outermost_context().is_service_ready_ = _event.is_ready_;

	if (!outermost_context().is_ready())
		return transit< not_seen >();

	return discard_event();
}

// State "not_requested"
not_requested::not_requested() {
}

sc::result not_requested::react(const ev_none &_event) {
	if (outermost_context().is_service_requested_)
		return transit< requested >();

	return discard_event();
}

sc::result not_requested::react(const ev_request_status_change &_event) {
	outermost_context().is_service_requested_ = _event.is_requested_;

	if (outermost_context().is_service_requested_)
		return transit< requested >();

	return discard_event();
}

// State "requested"
requested::requested() {
}

sc::result requested::react(const ev_request_status_change &_event) {
	outermost_context().is_service_requested_ = _event.is_requested_;

	if (!outermost_context().is_service_requested_)
		return transit< not_requested >();

	return discard_event();
}

sc::result requested::react(const ev_timeout_expired &_event) {
	return transit< requested >();
}

} // namespace client_state_machine
} // namespace sd
} // namespace vsomeip
