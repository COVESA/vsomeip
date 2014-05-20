//
// service_behavior.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include <vsomeip_internal/configuration.hpp>
#include <vsomeip_internal/daemon.hpp>
#include <vsomeip_internal/sd/service_discovery.hpp>
#include <vsomeip_internal/sd/service_state_machine.hpp>

namespace vsomeip {
namespace sd {
namespace service_state_machine {

machine::machine(service_discovery *_discovery)
	: discovery_(_discovery),
	  timer_service(_discovery->get_service()),
	  is_network_configured_(false),
	  is_service_ready_(false),
	  run_(0),
	  tcp_endpoint_(0),
	  udp_endpoint_(0) {

	configuration *the_configuration = configuration::request(get_application_name());

	std::cout << "SD ["
			  << (int)the_configuration->get_min_initial_delay() << ", "
			  << (int)the_configuration->get_max_initial_delay() << ", "
			  << (int)the_configuration->get_repetition_base_delay() << ", "
			  << (int)the_configuration->get_repetition_max() << ", "
			  << (int)the_configuration->get_cyclic_offer_delay() << "]" << std::endl;

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

machine::~machine() {
}

std::string machine::get_application_name() const {
	std::string name;

	if (discovery_) {
		daemon *its_owner = discovery_->get_owner();
		if (its_owner) {
			name = its_owner->get_name();
		}
	}

	return name;
}


void machine::send_offer_service(const endpoint *_target) {
	discovery_->send_offer_service(this, _target);
}

void machine::send_stop_offer_service() {
	discovery_->send_stop_offer_service(this);
}

void machine::timer_expired(const boost::system::error_code &_error) {
	if (!_error) {
		process_event(ev_timeout_expired());
	}
}

void machine::log(const std::string &_message) {
	std::cout << "Service ["
			  << std::hex << std::setfill('0') << std::setw(4) << service_
			  << "."
			  << std::hex << std::setfill('0') << std::setw(4) << instance_
			  << "] enters state \""
			  << _message
			  << "\""
			  << std::endl;
}

// state initial
initial::initial(my_context ctx) : sc::state< initial, machine >(ctx) {
	outermost_context().log("Initial");
}

sc::result initial::react(const ev_none &) {
	if (outermost_context().is_network_configured_ && outermost_context().is_service_ready_) {
		return transit< ready >();
	}

	return transit< not_ready >();
}

// state not_ready
not_ready::not_ready(my_context ctx) : sc::state< not_ready, machine >(ctx) {
	outermost_context().log("NotReady");
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
ready::ready(my_context ctx) : state< ready, machine, initializing >(ctx) {
	outermost_context().log("Ready");
}

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
		return transit< not_ready >();
	}

	return discard_event();
}

// state ready.initializing
initializing::initializing(my_context ctx) : sc::state< initializing, ready >(ctx) {
	outermost_context().log("Ready.Initializing");
	outermost_context().run_ = 0;
	outermost_context().start_timer(outermost_context().initial_delay_);
}

sc::result initializing::react(const ev_timeout_expired &_event) {
	outermost_context().send_offer_service();
	if (outermost_context().repetition_max_ > 0)
		return transit< repeating >();
	else
		return transit< announcing >();
}

// state ready.repeating
repeating::repeating(my_context ctx) : sc::state< repeating, ready >(ctx) {
	outermost_context().log("Ready.Repeating");

	outermost_context().start_timer(
		(2 << outermost_context().run_) * outermost_context().repetition_base_delay_);
	outermost_context().run_++;
}

sc::result repeating::react(const ev_find_service &_event) {
	outermost_context().send_offer_service(_event.source_);
	outermost_context().stop_timer();
	return transit< repeating >();
}

sc::result repeating::react(const ev_timeout_expired &_event) {
	if (outermost_context().run_ < outermost_context().repetition_max_) {
		outermost_context().send_offer_service();
		return transit< repeating >();
	} else {
		return transit< announcing >();
	}
}

// state ready.announcing
announcing::announcing(my_context ctx) : sc::state< announcing, ready >(ctx) {
	outermost_context().log("Ready.Announcing");
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

sc::result announcing::react(const ev_timeout_expired &_event) {
	outermost_context().send_offer_service();

	return transit< announcing >();
}

} // service_state_machine
} // namespace sd
} // namespace vsomeip

