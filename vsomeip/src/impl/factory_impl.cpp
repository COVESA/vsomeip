//
// factory_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#include <vsomeip/impl/factory_impl.hpp>
#include <vsomeip/impl/message_impl.hpp>
#include <vsomeip/impl/serializer_impl.hpp>
#include <vsomeip/impl/deserializer_impl.hpp>
#include <vsomeip/impl/endpoint_impl.hpp>
#include <vsomeip/impl/udp_client_impl.hpp>
#include <vsomeip/impl/udp_service_impl.hpp>
//#include <vsomeip/impl/tcp_client_impl.hpp>
//#include <vsomeip/impl/tcp_service_impl.hpp>


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

deserializer * factory_impl::create_deserializer(uint8_t *_data, uint32_t _length) const {
	if (0 == _data)
		return new deserializer_impl;

	return new deserializer_impl(_data, _length);
}

endpoint * factory_impl::create_endpoint() const {
	return new endpoint_impl;
}


client * factory_impl::create_client(const endpoint &_endpoint) const {
	client * new_client = 0;
	if (ip_protocol::UDP == _endpoint.get_protocol())
		new_client = new udp_client_impl(_endpoint);
//	else if (ip_protocol::TCP == protocol)
//		new_client = new tcp_client_impl(version);

	return new_client;
}

service * factory_impl::create_service(const endpoint &_endpoint) const {
	service * new_service = 0;
	if (ip_protocol::UDP == _endpoint.get_protocol())
		new_service = new udp_service_impl(_endpoint);

	return new_service;
}


}; // namespace vsomeip

