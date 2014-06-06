//
// events.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_EVENTS_HPP
#define VSOMEIP_INTERNAL_SD_EVENTS_HPP

#include <boost/statechart/event.hpp>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint;

namespace sd {

struct ev_none
		: boost::statechart::event< ev_none > {
};

struct ev_timeout_expired
		: boost::statechart::event< ev_timeout_expired > {
};

struct ev_status_change
		: boost::statechart::event< ev_status_change > {
};

/*
struct ev_network_status_change
		: boost::statechart::event< ev_network_status_change > {

	ev_network_status_change(bool _is_configured, bool _is_up)
		: is_configured_(_is_configured), is_up_(_is_up) {};

	bool is_configured_;
	bool is_up_;
};

struct ev_service_status_change
		: boost::statechart::event< ev_service_status_change > {

	ev_service_status_change(bool _is_ready) : is_ready_(_is_ready) {};

	bool is_ready_;
};
*/

struct ev_request_status_change
		: boost::statechart::event< ev_request_status_change > {

	ev_request_status_change(bool _is_requested) : is_requested_(_is_requested) {};

	bool is_requested_;
};

struct ev_find_service
		: boost::statechart::event< ev_find_service > {

	ev_find_service(service_id _service, instance_id _instance,
			        const endpoint *_source, bool _is_unicast_enabled)
		: service_(_service), instance_(_instance),
		  source_(_source), is_unicast_enabled_(_is_unicast_enabled) {};

	service_id service_;
	instance_id instance_;
	const endpoint *source_;
	bool is_unicast_enabled_;
};

struct ev_offer_service
		: boost::statechart::event< ev_offer_service > {
};

struct ev_stop_offer_service
		: boost::statechart::event< ev_stop_offer_service > {
};

struct ev_request_eventgroup
		: boost::statechart::event< ev_request_eventgroup > {

	ev_request_eventgroup(eventgroup_id _id)
		: id_(_id) {};

	eventgroup_id id_;
};

struct ev_release_eventgroup
	: boost::statechart::event< ev_release_eventgroup > {

	ev_release_eventgroup(eventgroup_id _id)
		: id_(_id) {};

	eventgroup_id id_;
};

struct ev_subscribe_eventgroup_ack
		: boost::statechart::event< ev_subscribe_eventgroup_ack > {

	ev_subscribe_eventgroup_ack(eventgroup_id _id, const endpoint *_address)
		: id_(_id), address_(_address) {};

	eventgroup_id id_;
	const endpoint *address_;
};

struct ev_unsubscribe_eventgroup_ack
		: boost::statechart::event< ev_unsubscribe_eventgroup_ack > {

	ev_unsubscribe_eventgroup_ack(eventgroup_id _id, const endpoint *_address)
		: id_(_id), address_(_address) {};

	eventgroup_id id_;
	const endpoint *address_;
};

struct ev_subscribe_eventgroup
		: boost::statechart::event< ev_subscribe_eventgroup > {

	ev_subscribe_eventgroup(const endpoint *_source, eventgroup_id _id, const endpoint *_address)
		: source_(_source), id_(_id), address_(_address) {};

	const endpoint *source_;
	eventgroup_id id_;
	const endpoint *address_;
};

struct ev_unsubscribe_eventgroup
	: boost::statechart::event< ev_unsubscribe_eventgroup > {

	ev_unsubscribe_eventgroup(const endpoint *_source, eventgroup_id _id, const endpoint *_address)
		: source_(_source), id_(_id), address_(_address) {};

	const endpoint *source_;
	eventgroup_id id_;
	const endpoint *address_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_EVENTS_HPP
