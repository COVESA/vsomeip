//
// ipv6_multicast_option_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#include <cstring>

#include <vsomeip/serializer.hpp>
#include <vsomeip/deserializer.hpp>
#include <vsomeip/constants.hpp>
#include <vsomeip/service_discovery/internal/ipv6_multicast_option_impl.hpp>

namespace vsomeip {
namespace service_discovery {

ipv6_multicast_option_impl::ipv6_multicast_option_impl() {
	length_ = (1 + 16 + 1 + 1 + 2);
	type_ = option_type::IP6_MULTICAST;
	memset(address_, 0, sizeof(address_));
	port_ = 0;
	protocol_ = ip_protocol::UDP;
}

ipv6_multicast_option_impl::~ipv6_multicast_option_impl() {
}

bool ipv6_multicast_option_impl::operator ==(const option &_other) const {
	if (_other.get_type() != option_type::IP6_MULTICAST)
		return false;

	const ipv6_multicast_option_impl& other
		= reinterpret_cast<const ipv6_multicast_option_impl&>(_other);

	return (address_ == other.address_
		 && port_ == other.port_
		 && protocol_ == other.protocol_);
}

void ipv6_multicast_option_impl::set_protocol(ip_protocol _protocol) {
	// intentionally left empty
}

} // namespace service_discovery
} // namespace vsomeip



