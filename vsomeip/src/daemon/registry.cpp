//
// registration.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/service_discovery/internal/registry.hpp>

namespace vsomeip {
namespace service_discovery {

registry::service * registry::add(service_id _service_id, instance_id _instance_id) {
	registry::service *found_service_instance = search(_service_id, _instance_id);
	if (found_service_instance)
		return found_service_instance;

	registry::service s;
	s.service_id_ = _service_id;
	s.instance_id_ = _instance_id;
	std::map< instance_id, service >& found_service = data_[_service_id];
	found_service[_instance_id] = s;

	return 0;
}

registry::service * registry::search(service_id _service_id, instance_id _instance_id) {
	auto found_service = data_.find(_service_id);
	if (found_service == data_.end())
		return 0;

	if (_instance_id == 0xFFFF) {
		return &(found_service->second.begin()->second);
	}

	auto found_instance = found_service->second.find(_instance_id);
	if (found_instance == found_service->second.end())
		return 0;

	return &found_instance->second;
}

} // namespace service_discovery
} // namespace vsomeip
