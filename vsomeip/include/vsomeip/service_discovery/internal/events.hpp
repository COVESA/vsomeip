//
// events.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_INTERNAL_EVENTS_HPP
#define VSOMEIP_SERVICE_DISCOVERY_INTERNAL_EVENTS_HPP

#include <boost/statechart/event.hpp>

namespace vsomeip {

class endpoint; // we need to know where the "message" comes from...

namespace service_discovery {

struct ev_none
		: boost::statechart::event< ev_none > {
};

struct ev_timeout
		: boost::statechart::event< ev_timeout > {
};

struct ev_network_status_change
		: boost::statechart::event< ev_timeout > {
	bool is_configured_;
	bool is_up_;
};

struct ev_service_status_change
		: boost::statechart::event< ev_service_status_change > {
	bool is_ready_;
};

struct ev_request_status_change
		: boost::statechart::event< ev_request_status_change > {
	bool is_requested_;
};

struct ev_find_service
		: boost::statechart::event< ev_find_service > {

	ev_find_service(endpoint *_source) : source_(_source) {};

	endpoint *source_;
};

struct ev_offer_service
		: boost::statechart::event< ev_offer_service > {
};

struct ev_stop_offer_service
		: boost::statechart::event< ev_stop_offer_service > {
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_EVENTS_HPP
