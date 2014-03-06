//
// service_administrator_behavior.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include <vsomeip/service_discovery/configuration.hpp>
#include <vsomeip/service_discovery/internal/service_administrator_behavior.hpp>

namespace vsomeip {
namespace service_discovery {

namespace sc = boost::statechart;

service_administrator_behavior::service_administrator_behavior()
	: is_network_configured_(false),
	  is_service_ready_(false),
	  run_(0) {

	configuration * the_configuration = configuration::get_instance();

	boost::random::mt19937 random_generator;
	boost::random::uniform_int_distribution<> distribution(
		the_configuration->get_min_initial_delay(),
		the_configuration->get_max_initial_delay()
	);

	initial_delay_ = distribution(random_generator);
	repetition_base_delay_ = the_configuration->get_repetition_base_delay();
	repetition_max_ = the_configuration->get_repetition_max();
	cyclic_offer_delay_ = the_configuration->get_cyclic_offer_delay();
};

// state initial
initial::initial() {
	post_event( ev_none() );
}

sc::result initial::react(const ev_none &) {
	if (outermost_context().is_network_configured_ && outermost_context().is_service_ready_) {
		return transit< ready >();
	}

	return transit< not_ready >();
}

// state not_ready
not_ready::not_ready() {
}

sc::result not_ready::react(const ev_network_status_change &_event) {
	outermost_context().is_network_configured_ = _event.is_configured_;

	if (outermost_context().is_network_configured_ && outermost_context().is_service_ready_) {
		return transit< ready >();
	}

	return discard_event();
}

sc::result not_ready::react(const ev_service_status_change &_event) {
	outermost_context().is_service_ready_ = _event.is_ready_;

	if (outermost_context().is_network_configured_ && outermost_context().is_service_ready_) {
		return transit< ready >();
	}

	return discard_event();
}

// state ready
sc::result ready::react(const ev_network_status_change &_event) {
	outermost_context().is_network_configured_ = _event.is_configured_;

	if (!outermost_context().is_network_configured_ || !outermost_context().is_service_ready_) {
		outermost_context().stop_timer();
		return transit< not_ready >();
	}

	return discard_event();
}

sc::result ready::react(const ev_service_status_change &_event) {
	outermost_context().is_service_ready_ = _event.is_ready_;

	if (!outermost_context().is_network_configured_ || !outermost_context().is_service_ready_) {
		outermost_context().stop_timer();
		outermost_context().send_stop_offer_service();
		return transit< ready >();
	}

	return discard_event();
}

// state ready.initializing
initializing::initializing() {
	outermost_context().run_ = 0;
	outermost_context().start_timer(outermost_context().initial_delay_);
}

sc::result initializing::react(const ev_timeout &) {
	outermost_context().send_offer_service();
	if (outermost_context().repetition_max_ > 0)
		return transit< repeating >();
	else
		return transit< announcing >();
}

// state ready.repeating
repeating::repeating() {
	outermost_context().start_timer(
		(2 << outermost_context().run_) * outermost_context().repetition_base_delay_);
	outermost_context().run_++;
}

sc::result repeating::react(const ev_find_service &_event) {
	outermost_context().send_offer_service(_event.source_);
	outermost_context().stop_timer();
	return transit< repeating >();
}

sc::result repeating::react(const ev_timeout &) {
	if (outermost_context().run_ < outermost_context().repetition_max_) {
		outermost_context().send_offer_service();
		return transit< repeating >();
	} else {
		return transit< announcing >();
	}
}

// state ready.announcing
announcing::announcing() {
	outermost_context().start_timer(
		outermost_context().cyclic_offer_delay_);
}

sc::result announcing::react(const ev_find_service &_event) {
	if (outermost_context().expired_from_now() < outermost_context().cyclic_offer_delay_) {
		outermost_context().send_offer_service(_event.source_);
	} else {
		outermost_context().send_offer_service();
	}
	outermost_context().stop_timer();

	return transit< announcing >();
}

sc::result announcing::react(const ev_timeout &) {
	outermost_context().send_offer_service();
}


} // namespace service_discovery
} // namespace vsomeip
