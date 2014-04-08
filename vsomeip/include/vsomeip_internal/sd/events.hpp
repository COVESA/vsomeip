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

namespace vsomeip {

class endpoint;

namespace sd {

struct ev_none
		: boost::statechart::event< ev_none > {
};

struct ev_timeout_expired
		: boost::statechart::event< ev_timeout_expired > {
};

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

struct ev_request_status_change
		: boost::statechart::event< ev_request_status_change > {

	ev_request_status_change(bool _is_requested) : is_requested_(_is_requested) {};

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

} // namespace sd
} // namespace vsomeip



#endif // VSOMEIP_INTERNAL_SD_EVENTS_HPP
