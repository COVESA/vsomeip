// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include <vsomeip/configuration.hpp>
#include <vsomeip/logger.hpp>

#include "../include/service_discovery.hpp"
#include "../include/service_discovery_fsm.hpp"

namespace vsomeip {
namespace sd {

///////////////////////////////////////////////////////////////////////////////
// Machine
///////////////////////////////////////////////////////////////////////////////
namespace _sd {

fsm::fsm(service_discovery_fsm *_fsm): fsm_(_fsm), fsm_base(_fsm->get_io()) {
	configuration *its_configuration = configuration::get();
}

fsm::~fsm() {
}

void fsm::timer_expired(const boost::system::error_code &_error) {
	if (!_error) {
		process_event(ev_timeout());
	}
}

///////////////////////////////////////////////////////////////////////////////
// State "Inactive"
///////////////////////////////////////////////////////////////////////////////
inactive::inactive(my_context context): sc::state< inactive, fsm >(context) {
	VSOMEIP_DEBUG << "sd::inactive";
}

sc::result inactive::react(const ev_none &_event) {
	if (outermost_context().is_up_) {
		return transit< active >();
	}

	return discard_event();
}

sc::result inactive::react(const ev_status_change &_event) {
	outermost_context().is_up_ = _event.is_up_;
	if (outermost_context().is_up_) {
		return transit< active >();
	}

	return discard_event();
}

///////////////////////////////////////////////////////////////////////////////
// State "Active"
///////////////////////////////////////////////////////////////////////////////
active::active(my_context _context): sc::state< active, fsm, initial >(_context) {
	VSOMEIP_DEBUG << "sd::active";
}

active::~active() {
}

sc::result active::react(const ev_status_change &_event) {
	outermost_context().is_up_ = _event.is_up_;
	if (!outermost_context().is_up_)
		return transit< inactive >();

	return discard_event();
}

///////////////////////////////////////////////////////////////////////////////
// State "Active.Initial"
///////////////////////////////////////////////////////////////////////////////
initial::initial(my_context _context): sc::state< initial, active >(_context) {
	VSOMEIP_DEBUG << "sd::inactive.initial";
	outermost_context().start_timer(outermost_context().initial_delay_);
}

sc::result initial::react(const ev_timeout &_event) {
	return transit< repeat >();
}

///////////////////////////////////////////////////////////////////////////////
// State "Active.Repeat"
///////////////////////////////////////////////////////////////////////////////
repeat::repeat(my_context _context): sc::state< repeat, active >(_context) {
	VSOMEIP_DEBUG << "sd::inactive.repeat";
}

sc::result repeat::react(const ev_timeout &_event) {
	return discard_event();
}

sc::result repeat::react(const ev_find_service &_event) {
	return discard_event();
}

///////////////////////////////////////////////////////////////////////////////
// State "Active.Announce"
///////////////////////////////////////////////////////////////////////////////
announce::announce(my_context _context): sc::state< announce, active  >(_context) {
	VSOMEIP_DEBUG << "sd::inactive.announce";
}

sc::result announce::react(const ev_timeout &_event) {
	return discard_event();
}

sc::result announce::react(const ev_find_service &_event) {
	return discard_event();
}

} // namespace _sd

///////////////////////////////////////////////////////////////////////////////
// Interface
///////////////////////////////////////////////////////////////////////////////
service_discovery_fsm::service_discovery_fsm(
		const std::string &_name, service_discovery *_discovery)
	: name_(_name), discovery_(_discovery), fsm_(this) {

	std::shared_ptr< configuration > its_configuration
		= discovery_->get_configuration();

	boost::random::mt19937 its_generator;
	boost::random::uniform_int_distribution<> its_distribution(
		its_configuration->get_min_initial_delay(name_),
		its_configuration->get_max_initial_delay(name_)
	);
	fsm_.initial_delay_ = its_distribution(its_generator);

	fsm_.repetition_base_delay_
		= its_configuration->get_repetition_base_delay(name_);
	fsm_.repetition_max_
		= its_configuration->get_repetition_max(name_);

	fsm_.cyclic_offer_delay_
		= its_configuration->get_cyclic_offer_delay(name_);

	VSOMEIP_INFO << "SD configuration ["
			<< fsm_.initial_delay_ << ":"
			<< fsm_.repetition_base_delay_ << ":"
			<< (int)fsm_.repetition_max_ << ":"
			<< fsm_.cyclic_offer_delay_ << "]";
}

const std::string & service_discovery_fsm::get_name() const {
	return name_;
}

boost::asio::io_service & service_discovery_fsm::get_io() {
	return discovery_->get_io();
}

} // namespace sd
} // namespace vsomeip
