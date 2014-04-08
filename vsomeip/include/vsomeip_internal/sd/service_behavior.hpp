//
// service_behavior.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_SERVICE_BEHAVIOR_HPP
#define VSOMEIP_INTERNAL_SD_SERVICE_BEHAVIOR_HPP

#include <boost/mpl/list.hpp>
#include <boost/statechart/custom_reaction.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/transition.hpp>

#include <vsomeip_internal/sd/events.hpp>
#include <vsomeip_internal/sd/timer_service.hpp>

namespace mpl = boost::mpl;
namespace sc = boost::statechart;

namespace vsomeip {
namespace sd {

class registry;

namespace service_fsm {

struct initial;
struct behavior
		: sc::state_machine< behavior, initial >,
		  public timer_service {

	behavior(registry *_registry);
	virtual ~behavior();

	void send_offer_service(const endpoint *_target = 0);
	void send_stop_offer_service();

	void timer_expired(const boost::system::error_code &_error);

	uint32_t initial_delay_;

	uint32_t repetition_base_delay_;
	uint8_t repetition_max_;
	uint8_t run_;

	uint32_t cyclic_offer_delay_;

	bool is_network_configured_;
	bool is_service_ready_;

	registry *registry_;
};

struct initial
		: sc::simple_state< initial, behavior > {

	initial();

	typedef sc::custom_reaction< ev_none > reactions;

	sc::result react(const ev_none &_event);
};

struct not_ready
		: sc::simple_state< not_ready, behavior > {

	not_ready();

	typedef mpl::list<
		sc::custom_reaction< ev_network_status_change >,
		sc::custom_reaction< ev_service_status_change >
	> reactions;

	sc::result react(const ev_network_status_change &_event);
	sc::result react(const ev_service_status_change &_event);
};

struct initializing;
struct ready
		: boost::statechart::simple_state< ready, behavior, initializing > {

	ready() {};

	typedef mpl::list<
		sc::custom_reaction< ev_network_status_change >,
		sc::custom_reaction< ev_service_status_change >
	> reactions;

	sc::result react(const ev_network_status_change &_event);
	sc::result react(const ev_service_status_change &_event);
};

struct initializing
		: boost::statechart::simple_state< initializing, ready > {

	initializing();

	typedef sc::custom_reaction< ev_timeout_expired > reactions;

	sc::result react(const ev_timeout_expired &_event);
};

struct repeating
		: boost::statechart::simple_state< repeating, ready > {

	repeating();

	typedef mpl::list<
		sc::custom_reaction< ev_find_service >,
		sc::custom_reaction< ev_timeout_expired >
	> reactions;

	sc::result react(const ev_find_service &);
	sc::result react(const ev_timeout_expired &_event);
};

struct announcing
		: boost::statechart::simple_state< announcing, ready > {

	announcing();

	typedef mpl::list<
		sc::custom_reaction< ev_find_service >,
		sc::custom_reaction< ev_timeout_expired >
	> reactions;

	sc::result react(const ev_find_service &_event);
	sc::result react(const ev_timeout_expired &_event);
};

} // namespace _service
} // namespace sd
} // namespace vsomeip


#endif // VSOMEIP_INTERNAL_SD_SERVICE_BEHAVIOR_HPP
