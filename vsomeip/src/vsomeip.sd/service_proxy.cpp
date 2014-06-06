//
// service_proxy.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/endpoint.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip_internal/sd/eventgroup_service_proxy.hpp>
#include <vsomeip_internal/sd/group.hpp>
#include <vsomeip_internal/sd/service_proxy.hpp>

namespace vsomeip {
namespace sd {

service_proxy::service_proxy(group *_group, service_id _service, instance_id _instance)
	: group_(_group),
	  service_(_service), instance_(_instance),
	  major_version_(0x01), minor_version_(0x01),
	  ttl_(0),
	  is_local_(true), is_started_(false),
	  reliable_(0), unreliable_(0) {
}

service_id service_proxy::get_service() const {
	return service_;
}

instance_id service_proxy::get_instance() const {
	return instance_;
}

major_version service_proxy::get_major_version() const {
	return major_version_;
}

minor_version service_proxy::get_minor_version() const {
	return minor_version_;
}

time_to_live service_proxy::get_time_to_live() const {
	return ttl_;
}

const endpoint * service_proxy::get_reliable() const {
	return reliable_;
}

const endpoint * service_proxy::get_unreliable() const {
	return unreliable_;
}

bool service_proxy::is_local() const {
	return is_local_;
}

bool service_proxy::is_started() const {
	return is_started_;
}

void service_proxy::provide(const endpoint *_location) {
	if (ip_protocol::TCP == _location->get_protocol()) {
		reliable_ = _location;
	} else {
		unreliable_ = _location;
	}
}

void service_proxy::withdraw(const endpoint *_location) {
	if (reliable_ == _location) {
		reliable_ = 0;
	} else if (unreliable_ == _location) {
		unreliable_ = 0;
	}

	if (0 == reliable_ && 0 == unreliable_) {
		group_->on_service_withdraw();
	}
}

void service_proxy::start() {
	ttl_ = 0xFFFFFF;
	is_started_ = true;
	group_->on_service_started();
}

void service_proxy::stop() {
	ttl_ = 0;
	is_started_ = false;
	group_->on_service_stopped();
}

void service_proxy::provide_eventgroup(eventgroup_id _eventgroup, const endpoint *_multicast) {
	eventgroup_service_proxy *its_proxy = 0;

	auto found_proxy = eventgroups_.find(_eventgroup);
	if (found_proxy != eventgroups_.end()) {
		its_proxy = found_proxy->second.get();
	} else {
		its_proxy = new eventgroup_service_proxy(this, _eventgroup);
		eventgroups_[_eventgroup].reset(its_proxy);
	}

	its_proxy->set_multicast(_multicast);
}

void service_proxy::withdraw_eventgroup(eventgroup_id _eventgroup) {
	// TODO: send Nack to all subscribers!?
	eventgroups_.erase(_eventgroup);
}

void service_proxy::subscribe(
		eventgroup_id _eventgroup, const endpoint *_source,
		const endpoint *_reliable, const endpoint *_unreliable, bool _stop) {

	const endpoint *its_multicast = 0;
	auto found_eventgroup = eventgroups_.find(_eventgroup);
	if (found_eventgroup != eventgroups_.end()) {
		its_multicast = found_eventgroup->second->get_multicast();

		if (_stop) { // unsubscribe
			found_eventgroup->second->unsubscribe(_reliable, _unreliable);
		} else {
			found_eventgroup->second->subscribe(_reliable, _unreliable);
		}
	} else {
		_stop = true;
	}

	group_->on_subscribe_eventgroup(service_, instance_, _eventgroup, _source, its_multicast, _stop);
}

} // namespace sd
} // namespace vsomeip
