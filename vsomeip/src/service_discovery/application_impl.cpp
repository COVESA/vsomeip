//
// application_impl.cpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/internal/byteorder.hpp>
#include <vsomeip/service_discovery/internal/application_impl.hpp>
#include <vsomeip/service_discovery/internal/client_impl.hpp>
#include <vsomeip/service_discovery/internal/service_impl.hpp>

namespace vsomeip {
namespace service_discovery {

application_impl::~application_impl() {
}

client * application_impl::create_service_discovery_client(
				service_id _service, instance_id _instance) {
	client * new_client = 0;
	uint32_t service_instance = VSOMEIP_WORDS_TO_LONG(_service, _instance);

	std::map<uint32_t, client*>::iterator found
		= service_discovery_clients_.find(service_instance);
	if (found == service_discovery_clients_.end()) {
		new_client = new client_impl(_service, _instance, is_);
		service_discovery_clients_[service_instance] = new_client;
	} else {
		new_client = found->second;
	}

	return new_client;
}

service * application_impl::create_service_discovery_service(
		service_id _service, instance_id _instance, const endpoint *_source) {
	service * new_service = 0;
	std::map<const endpoint*, vsomeip::service*>::iterator found = services_.find(_source);
	if (found == services_.end()) {
		vsomeip::service * delegate = create_service(_source);
		new_service = new service_impl(delegate);
		services_[_source] = new_service;
	} else {
		new_service = dynamic_cast<service_impl *>(found->second);
	}

	if (0 != new_service) {
		new_service->register_service(_service, _instance);
	} else {
		// TODO: check for "new_service == 0" and issue an error if so
	}

	return new_service;
}

} // namespace service_discovery
} // namespace vsomeip

