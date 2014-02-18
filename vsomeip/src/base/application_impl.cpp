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

#include <vsomeip/factory.hpp>
#include <vsomeip/internal/application_impl.hpp>
#include <vsomeip/internal/endpoint_impl.hpp>
#include <vsomeip/internal/tcp_client_impl.hpp>
#include <vsomeip/internal/tcp_service_impl.hpp>
#include <vsomeip/internal/udp_client_impl.hpp>
#include <vsomeip/internal/udp_service_impl.hpp>

namespace vsomeip {

application_impl::~application_impl() {
}

client * application_impl::create_client(const endpoint *_target) {
	client * new_client = 0;
	std::map<const endpoint*, client*>::iterator found = clients_.find(_target);
	if (found == clients_.end()) {
		if (ip_protocol::UDP == _target->get_protocol())
			new_client = new udp_client_impl(
									factory::get_default_factory(),
									_target, is_);

		else if (ip_protocol::TCP == _target->get_protocol())
			new_client = new tcp_client_impl(
									factory::get_default_factory(),
									_target, is_);

		clients_[_target] = new_client;
	} else {
		new_client = found->second;
	}

	return new_client;
}

service * application_impl::create_service(const endpoint *_source) {
	service * new_service = 0;
	std::map<const endpoint*, service*>::iterator found = services_.find(_source);
	if (found == services_.end()) {
		if (ip_protocol::UDP == _source->get_protocol())
			new_service = new udp_service_impl(
									factory::get_default_factory(),
									_source, is_);

		else if (ip_protocol::TCP == _source->get_protocol())
			new_service = new tcp_service_impl(
									factory::get_default_factory(),
									_source, is_);

		services_[_source] = new_service;
	} else {
		new_service = found->second;
	}

	return new_service;
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

} // namespace vsomeip

