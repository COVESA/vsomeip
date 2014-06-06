//
// client_proxy.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <iostream>

#include <vsomeip/endpoint.hpp>
#include <vsomeip_internal/sd/client_proxy.hpp>
#include <vsomeip_internal/sd/eventgroup_client_proxy.hpp>
#include <vsomeip_internal/sd/group.hpp>
#include <vsomeip_internal/sd/service_discovery.hpp>

namespace vsomeip {
namespace sd {

client_proxy::client_proxy(group *_group, service_id _service, instance_id _instance)
	: group_(_group), service_(_service), instance_(_instance),
	  reliable_local_(0), reliable_remote_(0), unreliable_local_(0), unreliable_remote_(0) {
}

service_id client_proxy::get_service() const {
	return service_;
}

instance_id client_proxy::get_instance() const {
	return instance_;
}

major_version client_proxy::get_major_version() const {
	return 0x1;
}

minor_version client_proxy::get_minor_version() const {
	return 0x1;
}

time_to_live client_proxy::get_time_to_live() const {
	return 0x3600;
}

bool client_proxy::is_available() const {
	return (reliable_remote_ || unreliable_remote_);
}

void client_proxy::set_available(const endpoint *_reliable, const endpoint *_unreliable, bool _is_available) {
	if (_is_available) {
		if (0 != _reliable) {
			reliable_remote_ = _reliable;
			update_clients(_reliable, true);
		}

		if (0 != _unreliable) {
			unreliable_remote_ = _unreliable;
			update_clients(_unreliable, true);
		}
	} else {
		if (0 != _reliable && reliable_remote_ == _reliable) {
			reliable_remote_ = 0;
			update_clients(_reliable, false);
		}

		if (0 != _unreliable && unreliable_remote_ == _unreliable) {
			unreliable_remote_ = 0;
			update_clients(_unreliable, false);
		}

		if (0 == _reliable && 0 == _unreliable) {
			reliable_remote_ = unreliable_remote_ = 0;
			update_clients(0, false);
		}
	}
}

void client_proxy::update_clients(const endpoint *_location, bool _is_available) {
	for (auto a_client : requesters_)
		group_->get_owner()->update_client(a_client, service_, instance_, _location, _is_available);

	if (_is_available) {
		group_->on_offer_service(service_, instance_, _location, reliable_local_, unreliable_local_);
	} else {
		if (0 == reliable_remote_ && 0 == unreliable_remote_) {
			for (auto i : eventgroups_) {
				// TODO: leave Multicast group(s)
			}
		}
	}
}

void client_proxy::request(client_id _client) {
	requesters_.insert(_client);
	group_->on_service_requested();
}

void client_proxy::release(client_id _client) {
	requesters_.erase(_client);
	group_->on_service_released();
}

void client_proxy::subscribe(client_id _client, eventgroup_id _eventgroup) {
	eventgroup_client_proxy *its_eventgroup = 0;

	auto found_eventgroup = eventgroups_.find(_eventgroup);
	if (found_eventgroup != eventgroups_.end()) {
		its_eventgroup = found_eventgroup->second.get();
	} else {
		its_eventgroup = new eventgroup_client_proxy(this, _eventgroup);
		eventgroups_[_eventgroup].reset(its_eventgroup);
	}

	its_eventgroup->subscribe(_client);
}

void client_proxy::unsubscribe(client_id _client, eventgroup_id _eventgroup) {
	auto found_eventgroup = eventgroups_.find(_eventgroup);
	if (found_eventgroup != eventgroups_.end()) {
		found_eventgroup->second->unsubscribe(_client);
	}
}

const std::map< eventgroup_id, boost::shared_ptr< eventgroup_client_proxy > > & client_proxy::get_eventgroups() const {
	return eventgroups_;
}

void client_proxy::set_local(const endpoint *_reliable, const endpoint *_unreliable) {
	reliable_local_ = _reliable;
	unreliable_local_ = _unreliable;
}

} // namespace sd
} // namespace vsomeip
