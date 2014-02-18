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

#include <vsomeip/service_discovery/internal/application_impl.hpp>
#include <vsomeip/service_discovery/internal/client_impl.hpp>

namespace vsomeip {
namespace service_discovery {

application_impl::~application_impl() {
}

client * application_impl::create_service_discovery_client(service_id _service,
		instance_id _instance) {
	return new client_impl(_service, _instance, is_);
}

service * application_impl::create_service_discovery_service(
		service_id _service, instance_id _instance, const endpoint *_source) {
	return 0;
}

std::size_t application_impl::poll_one() {
	return is_.poll_one();
}

std::size_t application_impl::poll() {
	return is_.poll();
}

std::size_t application_impl::run() {
	return is_.run();
}

} // namespace service_discovery
} // namespace vsomeip

