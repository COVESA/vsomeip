// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/configuration.hpp>
#include <vsomeip/logger.hpp>

#include "../include/service_discovery_fsm.hpp"
#include "../include/service_discovery_host.hpp"
#include "../include/service_discovery_impl.hpp"
#include "../../endpoints/include/tcp_server_endpoint_impl.hpp"
#include "../../endpoints/include/udp_server_endpoint_impl.hpp"
#include "../../routing/include/servicegroup.hpp"

namespace vsomeip {
namespace sd {

service_discovery_impl::service_discovery_impl(service_discovery_host *_host) :
		host_(_host), io_(_host->get_io()), default_(
				std::make_shared < service_discovery_fsm > ("default", this)) {
}

service_discovery_impl::~service_discovery_impl() {
}

std::shared_ptr<configuration> service_discovery_impl::get_configuration() const {
	return host_->get_configuration();
}

boost::asio::io_service & service_discovery_impl::get_io() {
	return io_;
}

void service_discovery_impl::init() {
	std::shared_ptr<configuration> its_configuration =
			host_->get_configuration();
	if (its_configuration) {
		std::set < std::string > its_servicegroups =
				its_configuration->get_servicegroups();
		for (auto its_group : its_servicegroups) {
			if (its_group != "default"
					&& its_configuration->is_local_servicegroup(its_group)) {
				additional_[its_group] = std::make_shared
						< service_discovery_fsm > (its_group, this);
			}
		}
	} else {
		VSOMEIP_ERROR << "SD: no configuration found!";
	}

	// SD endpoint
	boost::asio::ip::address its_address = its_configuration->get_address();
	uint16_t its_port = its_configuration->get_service_discovery_port();
	if (its_configuration->get_service_discovery_protocol() == "tcp") {
		endpoint_ = std::make_shared< tcp_server_endpoint_impl >(
						shared_from_this(),
						boost::asio::ip::tcp::endpoint(its_address, its_port),
						io_);
	} else {
		endpoint_ = std::make_shared< udp_server_endpoint_impl >(
						shared_from_this(),
						boost::asio::ip::udp::endpoint(its_address, its_port),
						io_);
	}
}

void service_discovery_impl::start() {
	endpoint_->start();

	default_->start();
	for (auto &its_group : additional_) {
		its_group.second->start();
	}

	default_->process(ev_none());
	for (auto &its_group : additional_) {
		its_group.second->process(ev_none());
	}
}

void service_discovery_impl::stop() {
}

void service_discovery_impl::offer_service(service_t _service,
		instance_t _instance) {
	VSOMEIP_DEBUG << "sdi::offer_service [" << std::hex << _service << "."
			<< _instance << "]";
}

void service_discovery_impl::stop_offer_service(service_t _service,
		instance_t _instance) {
	VSOMEIP_DEBUG << "sdi::stop_offer_service [" << std::hex << _service << "."
			<< _instance << "]";
}

void service_discovery_impl::request_service(service_t _service,
		instance_t _instance, major_version_t _major, minor_version_t _minor,
		ttl_t _ttl) {
	VSOMEIP_DEBUG << "sdi::request_service [" << std::hex << _service << "."
			<< _instance << "]";
}

void service_discovery_impl::release_service(service_t _service,
		instance_t _instance) {
	VSOMEIP_DEBUG << "sdi::release_service [" << std::hex << _service << "."
			<< _instance << "]";
}

// Interface endpoint_host
void service_discovery_impl::on_message(const byte_t *_data, length_t _length,
		endpoint *_receiver) {
	VSOMEIP_DEBUG << "sdi::on_message";
}

} // namespace sd
} // namespace vsomeip
