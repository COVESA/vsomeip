//
// client_behavior.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <iomanip>

#include <vsomeip/application.hpp>
#include <vsomeip_internal/sd/client_state_machine.hpp>
#include <vsomeip_internal/sd/service_discovery.hpp>

namespace vsomeip {
namespace sd {
namespace client_state_machine {

machine::machine(service_discovery *_discovery)
	: timer_service(_discovery->get_service()),
	  discovery_(_discovery),
	  tcp_endpoint_(0),
	  udp_endpoint_(0) {
}

machine::~machine() {
}

void machine::announce_service_state(bool _is_available) {
	discovery_->send_service_state(service_, instance_, _is_available);
}

void machine::timer_expired(const boost::system::error_code &_error) {
	if (0 == _error) {
		process_event(ev_timeout_expired());
	}
}

void machine::log(const std::string &_message) {
#ifdef VSOMEIP_SD_DEBUG
	std::cout << "Client ["
			  << std::hex << std::setfill('0') << std::setw(4) << service_
			  << "."
			  << std::hex << std::setfill('0') << std::setw(4) << instance_
			  << "] enters state \""
			  << _message
			  << "\""
			  << std::endl;
#endif
}


// State "not_seen"
not_seen::not_seen(my_context ctx)
	: sc::state< not_seen, machine, waiting >(ctx) {
	outermost_context().log("NotSeen");
	outermost_context().announce_service_state(false);
}

sc::result not_seen::react(const ev_offer_service &_event) {
	return transit< seen >();
}

// State "waiting"
waiting::waiting(my_context ctx)
	: sc::state< waiting, not_seen >(ctx) {
	outermost_context().log("NotSeen.Waiting");
}

sc::result waiting::react(const ev_none &_event) {
	if (outermost_context().is_ready() && outermost_context().is_service_requested_)
		return transit< initializing >();

	return discard_event();
}

sc::result waiting::react(const ev_network_status_change &_event) {
	outermost_context().is_network_up_ = _event.is_up_;
	outermost_context().is_network_configured_ = _event.is_configured_;

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
initializing::initializing(my_context ctx)
	: sc::state< initializing, not_seen >(ctx) {
	outermost_context().log("NotSeen.Initializing");
}

sc::result initializing::react(const ev_timeout_expired &_event) {
	return transit< searching >();
}

// State "searching"
searching::searching(my_context ctx)
	: sc::state< searching, not_seen >(ctx) {
	outermost_context().log("Searching");
}

sc::result searching::react(const ev_timeout_expired &_event) {
	return transit< searching >();
}

// State "seen"
seen::seen(my_context ctx)
	: sc::state< seen, machine, not_requested >(ctx) {
	outermost_context().log("Seen");
	outermost_context().announce_service_state(true);
	post_event(ev_none());
}

sc::result seen::react(const ev_timeout_expired &_event) {
	return transit< not_seen >();
}

sc::result seen::react(const ev_stop_offer_service &_event) {
	return transit< not_seen >();
}

sc::result seen::react(const ev_network_status_change &_event) {
	outermost_context().is_network_up_ = _event.is_up_;
	outermost_context().is_network_configured_ = _event.is_configured_;

	if (!outermost_context().is_ready())
		return transit< not_seen >();

	return discard_event();
}

// State "not_requested"
not_requested::not_requested(my_context ctx)
	: sc::state< not_requested, seen >(ctx) {
	outermost_context().log("Seen.NotRequested");
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
requested::requested(my_context ctx)
	: sc::state< requested, seen >(ctx) {
	outermost_context().log("Seen.Requested");
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
