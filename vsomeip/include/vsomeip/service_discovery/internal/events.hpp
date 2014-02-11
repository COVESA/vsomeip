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

#include <boost/variant.hpp>

namespace vsomeip {
namespace service_discovery {

struct ev_timeout_expired {
};

struct ev_daemon_status_change {
	ev_daemon_status_change(bool _is_up) { is_up_ = _is_up; };
	bool is_up_;
};

struct ev_service_status_change {
	ev_service_status_change(bool _is_up) { is_up_ = _is_up; };
	bool is_up_;
};

struct ev_configuration_status_change {
	ev_configuration_status_change(bool _is_configured) {
		is_configured_ = _is_configured;
	}

	bool is_configured_;
};


struct ev_request_change {
	ev_request_change(bool _is_requested) {
		is_requested_ = _is_requested;
	};

	bool is_requested_;
};

struct ev_find_service {
};

struct ev_offer_service {
};

struct ev_stop_offer_service {
};

typedef boost::variant<
		ev_timeout_expired,
		ev_daemon_status_change,
		ev_service_status_change,
		ev_configuration_status_change,
		ev_request_change,
		ev_find_service,
		ev_offer_service,
		ev_stop_offer_service > event_variant;

template<typename statemachine>
struct ProcessEvent
	: public boost::static_visitor<> {
	ProcessEvent(statemachine &_sm) : sm_(_sm) {};

	template<typename event>
	void operator()(event &e) const { sm_.process_event(e); }

	statemachine &sm_;
};


} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_EVENTS_HPP
