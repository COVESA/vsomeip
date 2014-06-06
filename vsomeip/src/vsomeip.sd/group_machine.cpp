//
// service_group_machine.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/endpoint.hpp>
#include <vsomeip_internal/configuration.hpp>
#include <vsomeip_internal/daemon.hpp>
#include <vsomeip_internal/sd/group.hpp>
#include <vsomeip_internal/sd/group_machine.hpp>

namespace vsomeip {
namespace sd {
namespace _group {

machine::machine(group *_group)
	: group_(_group),
	  timer_service(_group->get_service()) {
};

machine::~machine() {
}

bool machine::is_active() const {
	return (group_->is_network_configured() && group_->is_started());
}

void machine::send(service_id _service, instance_id _instance, const endpoint *_target, bool _is_announcing) {
	group_->send_services_status(_service, _instance, _target, _is_announcing);
}

void machine::timer_expired(const boost::system::error_code &_error) {
	if (!_error) {
		process_event(ev_timeout_expired());
	}
}

void machine::log(const std::string &_message) {
#ifdef VSOMEIP_SD_LOG
	std::cout << "[" << group_->get_name() << "] " << _message << std::endl;
#endif
}

///////////////////////////////////////////////////////////////////////////////
// State "Inactive"
///////////////////////////////////////////////////////////////////////////////

inactive::inactive(my_context _context)
	: sc::state< inactive, machine >(_context) {
	outermost_context().log("Inactive");
	outermost_context().run_ = 0;
}

sc::result inactive::react(const ev_none &_event) {
	return discard_event();
}

sc::result inactive::react(const ev_status_change &_event) {
	if (outermost_context().is_active())
		return transit< active >();

	return discard_event();
}

///////////////////////////////////////////////////////////////////////////////
// State "Active"
///////////////////////////////////////////////////////////////////////////////

active::active(my_context _context)
	: sc::state< active, machine, initial >(_context) {
	outermost_context().log("Active");
}

active::~active() {
	outermost_context().stop_timer();
	outermost_context().send();
}

sc::result active::react(const ev_status_change &_event) {
	if (!outermost_context().is_active())
		return transit< inactive >();

	return discard_event();
}

///////////////////////////////////////////////////////////////////////////////
// State "Active.Initial"
///////////////////////////////////////////////////////////////////////////////

initial::initial(my_context _context)
	: sc::state< initial, active >(_context) {
	outermost_context().log("Initial");
	outermost_context().start_timer(outermost_context().initial_delay_);
}

sc::result initial::react(const ev_timeout_expired &_event) {
	if (outermost_context().repetition_max_ > 0)
		return transit< repeat >();

	return transit< announce >();
}

///////////////////////////////////////////////////////////////////////////////
// State "Active.Repeat"
///////////////////////////////////////////////////////////////////////////////

repeat::repeat(my_context _context)
	: sc::state< repeat, active >(_context) {
	outermost_context().log("Repeat");
	outermost_context().send();
	outermost_context().start_timer(outermost_context().repetition_base_delay_);
}

sc::result repeat::react(const ev_timeout_expired &_event) {
	if (outermost_context().run_ == outermost_context().repetition_max_)
		return transit< announce >();

	outermost_context().run_++;
	return transit< repeat >();
}

sc::result repeat::react(const ev_find_service &_event) {
	outermost_context().stop_timer();
	outermost_context().send();
	return transit< repeat >();
}

///////////////////////////////////////////////////////////////////////////////
// State "Active.Announce"
///////////////////////////////////////////////////////////////////////////////

announce::announce(my_context _context)
	: sc::state< announce, active >(_context) {
	outermost_context().log("Announce");
	outermost_context().send(VSOMEIP_ANY_SERVICE, VSOMEIP_ANY_INSTANCE, 0, true);
	outermost_context().start_timer(outermost_context().cyclic_offer_delay_);
}

sc::result announce::react(const ev_timeout_expired &_event) {
	return transit< announce >();
}

sc::result announce::react(const ev_find_service &_event) {
	// SIP_SD_89, SIP_SD_90, SIP_SD_91
	if (_event.is_unicast_enabled_ &&
		outermost_context().expired_from_now() > outermost_context().cyclic_offer_delay_ / 2) {
		outermost_context().send(_event.service_, _event.instance_, _event.source_, true);
	} else {
		outermost_context().send(VSOMEIP_ANY_SERVICE, VSOMEIP_ANY_INSTANCE, 0, true);
	}

	outermost_context().stop_timer();
	return transit< announce >();
}

} // _service_group
} // namespace sd
} // namespace vsomeip

