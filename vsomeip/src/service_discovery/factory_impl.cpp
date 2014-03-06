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

#include <boost/asio/io_service.hpp>

#include <vsomeip/endpoint.hpp>
#include <vsomeip/service_discovery/internal/factory_impl.hpp>
#include <vsomeip/service_discovery/internal/client_impl.hpp>
#include <vsomeip/service_discovery/internal/message_impl.hpp>
#include <vsomeip/service_discovery/internal/deserializer_impl.hpp>

namespace vsomeip {
namespace service_discovery {

factory* factory_impl::get_default_factory() {
	static factory_impl factory__;
	return &factory__;
}

factory_impl::~factory_impl() {
}

client * factory_impl::create_service_discovery_client() const {
	return new client_impl;
}

message * factory_impl::create_service_discovery_message() const {
	return new message_impl;
}

deserializer * factory_impl::create_deserializer() const {
	return new deserializer_impl;
}

} // namespace service_discovery
} // namespace vsomeip


