//
// service_state_machine.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_SERVICE_STATE_MACHINE_HPP
#define VSOMEIP_INTERNAL_SD_SERVICE_STATE_MACHINE_HPP

#include <iomanip>

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

namespace service_state_machine {

struct initial;
struct machine
		: sc::state_machine< machine, initial >,
		  public timer_service {

	machine(service_discovery *_discovery);
	virtual ~machine();

	std::string get_application_name() const;

	void send_offer_service(const endpoint *_target = 0);
	void send_stop_offer_service();

	void timer_expired(const boost::system::error_code &_error);

	void log(const std::string &_message);

	uint32_t initial_delay_;

	uint32_t repetition_base_delay_;
	uint8_t repetition_max_;
	uint8_t run_;

	uint32_t cyclic_offer_delay_;

	bool is_network_configured_;
	bool is_service_ready_;

	// Service data
	service_id service_;
	instance_id instance_;
	major_version major_;
	minor_version minor_;

	// endpoints
	const endpoint *tcp_endpoint_;
	const endpoint *udp_endpoint_;

	service_discovery *discovery_;
};

struct initial
		: sc::state< initial, machine > {

	initial(my_context ctx);

	typedef sc::custom_reaction< ev_none > reactions;

	sc::result react(const ev_none &_event);
};

struct not_ready
		: sc::state< not_ready, machine > {

	not_ready(my_context ctx);

	typedef mpl::list<
		sc::custom_reaction< ev_network_status_change >,
		sc::custom_reaction< ev_service_status_change >
	> reactions;

	sc::result react(const ev_network_status_change &_event);
	sc::result react(const ev_service_status_change &_event);
};

struct initializing;
struct ready
		: boost::statechart::state< ready, machine, initializing > {

	ready(my_context ctx);

	typedef mpl::list<
		sc::custom_reaction< ev_network_status_change >,
		sc::custom_reaction< ev_service_status_change >
	> reactions;

	sc::result react(const ev_network_status_change &_event);
	sc::result react(const ev_service_status_change &_event);
};

struct initializing
		: boost::statechart::state< initializing, ready > {

	initializing(my_context ctx);

	typedef sc::custom_reaction< ev_timeout_expired > reactions;

	sc::result react(const ev_timeout_expired &_event);
};

struct repeating
		: boost::statechart::state< repeating, ready > {

	repeating(my_context ctx);

	typedef mpl::list<
		sc::custom_reaction< ev_find_service >,
		sc::custom_reaction< ev_timeout_expired >
	> reactions;

	sc::result react(const ev_find_service &);
	sc::result react(const ev_timeout_expired &_event);
};

struct announcing
		: boost::statechart::state< announcing, ready > {

	announcing(my_context ctx);

	typedef mpl::list<
		sc::custom_reaction< ev_find_service >,
		sc::custom_reaction< ev_timeout_expired >
	> reactions;

	sc::result react(const ev_find_service &_event);
	sc::result react(const ev_timeout_expired &_event);
};

} // namespace service_state_machine

typedef service_state_machine::machine service_state_machine_t;

} // namespace sd
} // namespace vsomeip


#endif // VSOMEIP_INTERNAL_SD_SERVICE_STATE_MACHINE_HPP
