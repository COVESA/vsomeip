//
// service_behavior_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/service_discovery/factory.hpp>
#include <vsomeip/service_discovery/message.hpp>
#include <vsomeip/service_discovery/internal/daemon.hpp>
#include <vsomeip/service_discovery/internal/service_administrator.hpp>

namespace vsomeip {
namespace service_discovery {

service_administrator::service_administrator(
	daemon *_daemon, service_id _service, instance_id _instance)
	: daemon_(_daemon), service_(_service), instance_(_instance) {
}

void service_administrator::init() {

}

void service_administrator::start() {

}

void service_administrator::stop() {

}

void service_administrator::send_offer_service(endpoint *_target) {
	message * offer = factory::get_default_factory()->create_service_discovery_message();

	// set target address
	if (_target) {
		offer->set_endpoint(_target);
	} else {
		//offer->set_endpoint(daemon_->get_multicast_address());
	}

	// send message
	daemon_->send(offer);
}

void service_administrator::send_stop_offer_service() {
	message * stop_offer = factory::get_default_factory()->create_service_discovery_message();

	daemon_->send(stop_offer);
}

void service_administrator::timer_expired(const boost::system::error_code &_error) {
	if (!_error) {
		process_event(ev_timeout());
	}
}

} // namespace service_discovery
} // namespace vsomeip
