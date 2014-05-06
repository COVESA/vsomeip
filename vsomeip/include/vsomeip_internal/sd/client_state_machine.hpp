//
// client_state_machine.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_CLIENT_STATE_MACHINE_HPP
#define VSOMEIP_INTERNAL_SD_CLIENT_STATE_MACHINE_HPP

#include <boost/mpl/list.hpp>
#include <boost/statechart/custom_reaction.hpp>
#include <boost/statechart/state.hpp>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/transition.hpp>

#include <vsomeip_internal/sd/events.hpp>
#include <vsomeip_internal/sd/timer_service.hpp>

namespace mpl = boost::mpl;
namespace sc = boost::statechart;

namespace vsomeip {
namespace sd {

class service_discovery;

namespace client_state_machine {

struct not_seen;
struct machine
		: sc::state_machine< machine, not_seen >,
		  public timer_service {

	machine(service_discovery *_discovery);
	~machine();

	uint32_t initial_delay_;

	uint32_t repetition_base_delay_;
	uint8_t repetition_max_;
	uint8_t run_;

	uint32_t cyclic_offer_delay_;

	bool is_network_ready_;
	bool is_service_ready_;
	bool is_service_requested_;

	// Service data
	service_id service_;
	instance_id instance_;
	major_version major_;
	minor_version minor_;

	service_discovery *discovery_;

	inline bool is_ready() { return (is_network_ready_ && is_service_ready_); };

	void send_find_service(endpoint *_target = 0);

	void timer_expired(const boost::system::error_code &_error);
};

struct waiting;
struct not_seen
		: sc::state< not_seen, machine, waiting > {

	not_seen(my_context ctx);

	typedef sc::custom_reaction< ev_offer_service > reactions;

	sc::result react(const ev_offer_service &_event);
};

struct waiting
		: sc::state< waiting, not_seen > {

	waiting(my_context ctx);

	typedef mpl::list<
		sc::custom_reaction< ev_none >,
		sc::custom_reaction< ev_service_status_change >,
		sc::custom_reaction< ev_request_status_change >
	> reactions;

	sc::result react(const ev_none &_event);
	sc::result react(const ev_service_status_change &_event);
	sc::result react(const ev_request_status_change &_event);
};

struct initializing
		: sc::state< initializing, not_seen > {

	initializing(my_context ctx);

	typedef sc::custom_reaction< ev_timeout_expired > reactions;

	sc::result react(const ev_timeout_expired &_event);
};

struct searching
		: sc::state< searching, not_seen > {

	searching(my_context ctx);

	typedef sc::custom_reaction< ev_timeout_expired > reactions;

	sc::result react(const ev_timeout_expired &_event);
};


struct not_requested;
struct seen
		: sc::state< seen, machine, not_requested > {

	seen(my_context ctx);

	typedef mpl::list<
		sc::custom_reaction< ev_timeout_expired >,
		sc::custom_reaction< ev_stop_offer_service >,
		sc::custom_reaction< ev_service_status_change >
	> reactions;

	sc::result react(const ev_timeout_expired &_event);
	sc::result react(const ev_stop_offer_service &_event);
	sc::result react(const ev_service_status_change &_event);
};

struct not_requested
		: sc::state< not_requested, seen > {

	not_requested(my_context ctx);

	typedef mpl::list<
		sc::custom_reaction< ev_none >,
		sc::custom_reaction< ev_request_status_change >
	> reactions;

	sc::result react(const ev_none &_event);
	sc::result react(const ev_request_status_change &_event);
};

struct requested
		: sc::state< requested, seen > {

	requested(my_context ctx);

	typedef mpl::list<
		sc::custom_reaction< ev_request_status_change >,
		sc::custom_reaction< ev_timeout_expired >
	> reactions;

	sc::result react(const ev_request_status_change &_event);
	sc::result react(const ev_timeout_expired &_event);
};

} // namespace client_state_machine {

typedef client_state_machine::machine client_state_machine_t;

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_CLIENT_STATE_MACHINE_HPP
