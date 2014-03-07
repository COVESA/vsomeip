//
// endpoint_impl.cpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip_internal/endpoint_impl.hpp>

namespace vsomeip {

endpoint_impl::endpoint_impl(
		ip_address _address, ip_port _port,
		transport_protocol _protocol, transport_protocol_version _version)
	: protocol_(_protocol), version_(_version),
	  address_(_address), port_(_port) {
}

endpoint_impl::~endpoint_impl() {
}

ip_address endpoint_impl::get_address() const {
	return address_;
}

ip_port endpoint_impl::get_port() const {
	return port_;
}

transport_protocol endpoint_impl::get_protocol() const {
	return protocol_;
}


transport_protocol_version endpoint_impl::get_version() const {
	return version_;
}

} // namespace vsomeip





