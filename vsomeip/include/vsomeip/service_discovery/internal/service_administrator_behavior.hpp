/*
 * service_administrator_behavior.hpp
 *
 *  Created on: Feb 25, 2014
 *      Author: someip
 */

#ifndef VSOMEIP_SERVICE_DISCOVERY_INTERNAL_SERVICE_ADMINISTRATOR_BEHAVIOR_HPP
#define VSOMEIP_SERVICE_DISCOVERY_INTERNAL_SERVICE_ADMINISTRATOR_BEHAVIOR_HPP

#include <boost/mpl/list.hpp>
#include <boost/statechart/custom_reaction.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/transition.hpp>

#include <vsomeip/service_discovery/internal/events.hpp>
#include <vsomeip/service_discovery/internal/timer_service.hpp>

namespace vsomeip {

class endpoint;

namespace service_discovery {

namespace mpl = boost::mpl;
namespace sc = boost::statechart;

struct initial;
struct service_administrator_behavior
		: sc::state_machine< service_administrator_behavior, initial >,
		  public timer_service {

	service_administrator_behavior();

	uint32_t initial_delay_;

	uint32_t repetition_base_delay_;
	uint8_t repetition_max_;
	uint8_t run_;

	uint32_t cyclic_offer_delay_;

	bool is_network_configured_;
	bool is_service_ready_;

	virtual void send_offer_service(endpoint *_target = 0) = 0;
	virtual void send_stop_offer_service() = 0;
};

struct initial
		: sc::simple_state< initial, service_administrator_behavior > {

	initial();

	typedef sc::custom_reaction< ev_none > reactions;

	sc::result react(const ev_none &);
};

struct not_ready
		: sc::simple_state< not_ready, service_administrator_behavior > {

	not_ready();

	typedef mpl::list<
		sc::custom_reaction< ev_network_status_change >,
		sc::custom_reaction< ev_service_status_change > > reactions;

	sc::result react(const ev_network_status_change &);
	sc::result react(const ev_service_status_change &);
};

struct initializing;
struct ready
		: boost::statechart::simple_state< ready, service_administrator_behavior, initializing > {

	ready() {};

	typedef mpl::list<
		sc::custom_reaction< ev_network_status_change >,
		sc::custom_reaction< ev_service_status_change > > reactions;

	sc::result react(const ev_network_status_change &);
	sc::result react(const ev_service_status_change &);
};

struct initializing
		: boost::statechart::simple_state< initializing, ready > {

	initializing();

	typedef sc::custom_reaction< ev_timeout > reactions;

	sc::result react(const ev_timeout &);
};

struct repeating
		: boost::statechart::simple_state< repeating, ready > {

	repeating();

	typedef mpl::list<
		sc::custom_reaction< ev_find_service >,
		sc::custom_reaction< ev_timeout > > reactions;

	sc::result react(const ev_find_service &);
	sc::result react(const ev_timeout &);
};

struct announcing
		: boost::statechart::simple_state< announcing, ready > {

	announcing();

	typedef mpl::list<
		sc::custom_reaction< ev_find_service >,
		sc::custom_reaction< ev_timeout > > reactions;

	sc::result react(const ev_find_service &);
	sc::result react(const ev_timeout &);
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_SERVICE_ADMINISTRATOR_BEHAVIOR_HPP
