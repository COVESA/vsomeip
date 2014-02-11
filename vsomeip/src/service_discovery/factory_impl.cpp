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
#include <vsomeip/service_discovery/internal/message_impl.hpp>
#include <vsomeip/service_discovery/internal/deserializer_impl.hpp>
#include <vsomeip/service_discovery/internal/client_impl.hpp>

namespace vsomeip {
namespace service_discovery {

factory* factory_impl::get_default_factory() {
	static factory_impl factory__;
	return &factory__;
}

factory_impl::~factory_impl() {
}

message * factory_impl::create_service_discovery_message() const {
	return new message_impl;
}

deserializer * factory_impl::create_deserializer() const {
	return new deserializer_impl;
}

vsomeip::client * factory_impl::create_client(const endpoint *_target) const {
	return vsomeip::factory_impl::create_client(_target);
}

vsomeip::client * factory_impl::create_client(service_id _service_id,
		 	 	 	 	 	 	 	 	 instance_id _instance_id,
		 	 	 	 	 	 	 	 	 major_version _major_version,
		 	 	 	 	 	 	 	 	 time_to_live _time_to_live) const {
	return new client_impl(_service_id, _instance_id,
							_major_version, _time_to_live,
							new boost::asio::io_service);
}

vsomeip::service * factory_impl::create_service(const endpoint *_endpoint) const {
	vsomeip::service * new_service = 0;
#if 0
	if (ip_protocol::UDP == _endpoint->get_protocol())
		new_service = new udp_service_impl(_endpoint);
	else if (ip_protocol::TCP == _endpoint->get_protocol())
		new_service = new tcp_service_impl(_endpoint);
#endif
	return new_service;
}


} // namespace service_discovery
} // namespace vsomeip


