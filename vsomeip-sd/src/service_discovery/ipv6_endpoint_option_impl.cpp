//
// ipv6_endpoint_option_impl.cpp
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//
#include <cstring>

#include <vsomeip/serializer.hpp>
#include <vsomeip/deserializer.hpp>
#include <vsomeip/impl/constants_impl.hpp>
#include <vsomeip/service_discovery/impl/ipv6_endpoint_option_impl.hpp>

namespace vsomeip {
namespace service_discovery {

ipv6_endpoint_option_impl::ipv6_endpoint_option_impl() {
	length_ = (1 + 16 + 1 + 1 + 2);
	type_ = option_type::IP6_ENDPOINT;
	memset(address_, 0, sizeof(address_));
	port_ = 0;
	protocol_ = ip_protocol::UNKNOWN;
}

ipv6_endpoint_option_impl::~ipv6_endpoint_option_impl() {
}

bool ipv6_endpoint_option_impl::operator ==(const option &_other) const {
	if (_other.get_type() != option_type::IP6_ENDPOINT)
		return false;

	const ipv6_endpoint_option_impl& other
		= reinterpret_cast<const ipv6_endpoint_option_impl&>(_other);

	return (address_ == other.address_
		 && port_ == other.port_
		 && protocol_ == other.protocol_);
}

const ipv6_address& ipv6_endpoint_option_impl::get_address() const {
	return address_;
}

void ipv6_endpoint_option_impl::set_address(const ipv6_address &_address) {
	memcpy(address_, _address, sizeof(_address));
}

ip_port ipv6_endpoint_option_impl::get_port() const {
	return port_;
}

void ipv6_endpoint_option_impl::set_port(ip_port _port) {
	port_ = _port;
}

ip_protocol ipv6_endpoint_option_impl::get_protocol() const {
	return protocol_;
}

void ipv6_endpoint_option_impl::set_protocol(ip_protocol _protocol) {
	protocol_ = _protocol;
}

bool ipv6_endpoint_option_impl::serialize(vsomeip::serializer *_to) const {
	bool is_successful = option_impl::serialize(_to);

	is_successful = is_successful && _to->serialize(address_, sizeof(address_));
	is_successful = is_successful && _to->serialize(vsomeip_protocol_reserved_byte);
	is_successful = is_successful && _to->serialize(static_cast<uint8_t>(protocol_));
	is_successful = is_successful && _to->serialize(port_);

	return is_successful;
}

bool ipv6_endpoint_option_impl::deserialize(vsomeip::deserializer *_from) {
	bool is_successful = option_impl::deserialize(_from);

	is_successful = is_successful && _from->deserialize(address_, sizeof(address_));

	uint8_t reserved;
	is_successful = is_successful && _from->deserialize(reserved);

	uint8_t protocol = 0xFF;
	is_successful = is_successful && _from->deserialize(protocol);
	protocol_ = static_cast<ip_protocol>(protocol);

	is_successful = is_successful && _from->deserialize(port_);

	return is_successful;
}

} // namespace service_discovery
} // namespace vsomeip



