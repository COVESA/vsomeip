//
// factory_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#include <vsomeip/impl/factory_impl.hpp>
#include <vsomeip/impl/message_impl.hpp>
#include <vsomeip/impl/serializer_impl.hpp>
#include <vsomeip/impl/deserializer_impl.hpp>
#include <vsomeip/impl/endpoint_impl.hpp>
#include <vsomeip/impl/udp_client_impl.hpp>
#include <vsomeip/impl/udp_service_impl.hpp>
#include <vsomeip/impl/tcp_client_impl.hpp>
#include <vsomeip/impl/tcp_service_impl.hpp>

namespace vsomeip {

factory * factory_impl::get_default_factory() {
	static factory_impl factory__;
	return &factory__;
}

factory_impl::~factory_impl() {
}

message * factory_impl::create_message() const {
	return new message_impl;
}

serializer * factory_impl::create_serializer() const {
	return new serializer_impl;
}

deserializer * factory_impl::create_deserializer() const {
	return new deserializer_impl;
}

endpoint * factory_impl::create_endpoint(ip_address _address, ip_port _port,
		ip_protocol _protocol, ip_version _version) {

	uint32_t identifier = (((uint32_t) _protocol) << 24
			| ((uint32_t) _version) << 16 | ((uint32_t) _port));

	std::map<std::string, endpoint *>& matching_identifier =
			endpoints_[identifier];

	endpoint *requested_endpoint = 0;

	auto endpoint_iterator = matching_identifier.find(_address);
	if (endpoint_iterator == matching_identifier.end())
	{
		requested_endpoint = new endpoint_impl(
				_address, _port, _protocol, _version);
		matching_identifier[_address] = requested_endpoint;
	} else {
		requested_endpoint = endpoint_iterator->second;
	}

	return requested_endpoint;
}

client * factory_impl::create_client(const endpoint *_endpoint) const {
	client * new_client = 0;
	if (ip_protocol::UDP == _endpoint->get_protocol())
		new_client = new udp_client_impl(_endpoint);
	else if (ip_protocol::TCP == _endpoint->get_protocol())
		new_client = new tcp_client_impl(_endpoint);

	return new_client;
}

service * factory_impl::create_service(const endpoint *_endpoint) const {
	service * new_service = 0;
	if (ip_protocol::UDP == _endpoint->get_protocol())
		new_service = new udp_service_impl(_endpoint);
	else if (ip_protocol::TCP == _endpoint->get_protocol())
		new_service = new tcp_service_impl(_endpoint);

	return new_service;
}

} // namespace vsomeip

