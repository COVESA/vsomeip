//
// ipv4_multicast_option_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/serializer.hpp>
#include <vsomeip/deserializer.hpp>
#include <vsomeip_internal/constants.hpp>
#include <vsomeip_internal/service_discovery/ipv4_multicast_option_impl.hpp>

namespace vsomeip {
namespace service_discovery {

ipv4_multicast_option_impl::ipv4_multicast_option_impl() {
	length_ = (1 + 4 + 1 + 1 + 2);
	type_ = option_type::IP4_MULTICAST;
	address_ = 0;
	port_ = 0;
	protocol_ = transport_protocol::UDP;
}

ipv4_multicast_option_impl::~ipv4_multicast_option_impl() {
}

bool ipv4_multicast_option_impl::operator ==(const option &_other) const {
	if (_other.get_type() != option_type::IP4_MULTICAST)
		return false;

	const ipv4_multicast_option_impl& other =
			reinterpret_cast<const ipv4_multicast_option_impl&>(_other);

	return (address_ == other.address_ && port_ == other.port_
			&& protocol_ == other.protocol_);
}

void ipv4_multicast_option_impl::set_protocol(transport_protocol _protocol) {
	// intentionally left empty
}

} // namespace service_discovery
} // namespace vsomeip

