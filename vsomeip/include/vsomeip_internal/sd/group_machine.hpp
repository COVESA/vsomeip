//
// group_machine.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_GROUP_MACHINE_HPP
#define VSOMEIP_INTERNAL_SD_GROUP_MACHINE_HPP

#include <iomanip>
#include <set>

#include <boost/mpl/list.hpp>
#include <boost/statechart/custom_reaction.hpp>
#include <boost/statechart/state.hpp>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/transition.hpp>

#include <vsomeip/config.hpp>
#include <vsomeip_internal/sd/events.hpp>
#include <vsomeip_internal/sd/timer_service.hpp>

namespace mpl = boost::mpl;
namespace sc = boost::statechart;

namespace vsomeip {
namespace sd {

class group;

namespace _group {

struct inactive;
struct machine
	: sc::state_machine< machine, inactive >,
	  public timer_service {

	machine(group *_group);
	virtual ~machine();

	bool is_active() const;

	void send(service_id _service = VSOMEIP_ANY_SERVICE,
			  instance_id _instance = VSOMEIP_ANY_INSTANCE,
			  const endpoint *_target = 0,
			  bool _is_announcing = false);

	void timer_expired(const boost::system::error_code &_error);
	void log(const std::string &_message);

	// Delays within the different phases
	uint32_t initial_delay_;
	uint32_t repetition_base_delay_;
	uint32_t cyclic_offer_delay_;

	// The management for a group of service that share
	// the same behavior with regard to service discovery
	// timings.
	group *group_;

	// Repetitions
	uint8_t repetition_max_;
	uint8_t run_;
};

struct inactive
		: sc::state< inactive, machine > {

	inactive(my_context _context);

	typedef mpl::list<
		sc::custom_reaction< ev_none >,
		sc::custom_reaction< ev_status_change >
	> reactions;

	sc::result react(const ev_none &_event);
	sc::result react(const ev_status_change &_event);
};

struct initial;
struct active
		: sc::state< active, machine, initial > {

	active(my_context _context);
	~active();

	typedef sc::custom_reaction< ev_status_change > reactions;

	sc::result react(const ev_status_change &_event);
};

struct initial
		: sc::state< initial, active > {

	initial(my_context _context);

	typedef sc::custom_reaction< ev_timeout_expired > reactions;

	sc::result react(const ev_timeout_expired &_event);
};

struct repeat
		: sc::state< repeat, active > {

	repeat(my_context _context);

	typedef mpl::list<
		sc::custom_reaction< ev_timeout_expired >,
		sc::custom_reaction< ev_find_service >
	> reactions;

	sc::result react(const ev_timeout_expired &_event);
	sc::result react(const ev_find_service &_event);
};

struct announce
		: sc::state< announce, active > {

	announce(my_context _context);

	typedef mpl::list<
		sc::custom_reaction< ev_timeout_expired >,
		sc::custom_reaction< ev_find_service >
	> reactions;

	sc::result react(const ev_timeout_expired &_event);
	sc::result react(const ev_find_service &_event);
};

} // namespace _group

typedef _group::machine group_machine;

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_GROUP_MACHINE_HPP
