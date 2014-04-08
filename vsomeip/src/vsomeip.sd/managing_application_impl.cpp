//
// managing_application_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/sd/factory.hpp>
#include <vsomeip_internal/tcp_client_impl.hpp>
#include <vsomeip_internal/udp_client_impl.hpp>
#include <vsomeip_internal/sd/managing_application_impl.hpp>

namespace vsomeip {
namespace sd {

managing_application_impl::managing_application_impl(const std::string &_name)
	: vsomeip::managing_application_impl(_name), sd_(0) {
}

managing_application_impl::~managing_application_impl() {
}

void managing_application_impl::init(int _options_count, char **_options) {
	// TODO: read the following variables from configuration
	ip_protocol service_discovery_protocol = ip_protocol::UDP;
	ip_address service_discovery_address = "127.0.0.1";
	ip_port service_discovery_port = 30490;

	// get SD endpoint
	endpoint *service_discovery_endpoint
		= factory::get_instance()->get_endpoint(
				service_discovery_address,
				service_discovery_port,
				service_discovery_protocol
		  );

	// initialize the client
	client * service_discovery_client = 0;
	if (ip_protocol::TCP == service_discovery_protocol) {
		service_discovery_client = new tcp_client_impl(this, service_discovery_endpoint);
	} else {
		service_discovery_client = new udp_client_impl(this, service_discovery_endpoint);
	}

	sd_.reset(service_discovery_client);

	// read other configuration data
	vsomeip::managing_application_impl::init(_options_count, _options);
}

bool managing_application_impl::request_service(
		service_id _service, instance_id _instance, const endpoint *) {

	auto find_service = used_clients_.find(_service);
	if (find_service != used_clients_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance == find_service->second.end()) {
			find_service->second[_instance].reset(new client_fsm::behavior(this));
		}
	} else {
		used_clients_[_service][_instance].reset(new client_fsm::behavior(this));
	}

	return true;
}

bool managing_application_impl::release_service(
		service_id _service, instance_id _instance) {

	auto find_service = used_clients_.find(_service);
	if (find_service != used_clients_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance != find_service->second.end()) {
			find_service->second.erase(find_instance);
		}

		if (0 == find_service->second.size()) {
			used_clients_.erase(find_service);
		}
	}

	return true;
}

bool managing_application_impl::provide_service(
		service_id _service, instance_id _instance, const endpoint *_location) {
	return true;
}

bool managing_application_impl::withdraw_service(
		service_id _service, instance_id _instance, const endpoint *_location) {
	return true;
}

bool managing_application_impl::start_service(
		service_id _service, instance_id _instance) {
	return true;
}

bool managing_application_impl::stop_service(
		service_id _service, instance_id _instance) {
	return true;
}

} // namespace sd
} // namespace vsomeip

