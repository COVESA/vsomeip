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

#define VSOMEIP_SD_DEBUG

#include <set>

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

class client_group;

namespace _client_group {

struct eventgroup_machine;

struct not_seen;
struct machine
		: sc::state_machine< machine, not_seen >,
		  public timer_service {

	machine(client_group *_group);
	~machine();

	eventgroup_machine * find_eventgroup_machine(eventgroup_id _eventgroup);
	void request_eventgroup(client_id _client, eventgroup_id _eventgroup);
	void release_eventgroup(client_id _client, eventgroup_id _eventgroup);

	inline bool is_ready() { return (is_network_up_ && is_network_configured_); };

	void announce_service_state(bool _is_available);

	void send_find_service();
	void send_subscribe();

	void timer_expired(const boost::system::error_code &_error);

	void log(const std::string &_message);

	uint32_t initial_delay_;

	uint32_t repetition_base_delay_;
	uint8_t repetition_max_;
	uint8_t run_;

	uint32_t cyclic_offer_delay_;

	bool is_network_up_;
	bool is_network_configured_;

	// Service data
	service_id service_;
	instance_id instance_;
	major_version major_;
	minor_version minor_;

	const endpoint *sd_;

	std::map< eventgroup_id, eventgroup_machine * > eventgroups_;

	service_discovery *discovery_;
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
		sc::custom_reaction< ev_network_status_change >,
		sc::custom_reaction< ev_request_status_change >
	> reactions;

	sc::result react(const ev_none &_event);
	sc::result react(const ev_network_status_change &_event);
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
struct not_subscribed;
struct seen
		: sc::state< seen, machine, not_requested > {

	seen(my_context ctx);

	typedef mpl::list<
		sc::custom_reaction< ev_timeout_expired >,
		sc::custom_reaction< ev_stop_offer_service >,
		sc::custom_reaction< ev_network_status_change >
	> reactions;

	sc::result react(const ev_timeout_expired &_event);
	sc::result react(const ev_stop_offer_service &_event);
	sc::result react(const ev_network_status_change &_event);
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

} // namespace _client_group

typedef _client_group::machine client_state_machine;

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_CLIENT_STATE_MACHINE_HPP
