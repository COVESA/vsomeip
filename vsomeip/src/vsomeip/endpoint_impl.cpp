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
#include <vsomeip_internal/serializer.hpp>
#include <vsomeip_internal/deserializer.hpp>

namespace vsomeip {

endpoint_impl::endpoint_impl(
		boost::asio::ip::address _address, ip_port _port, ip_protocol _protocol)
	: address_(_address),
	  port_(_port),
	  protocol_(_protocol) {
}

endpoint_impl::~endpoint_impl() {
}

ip_address endpoint_impl::get_address() const {
	return address_.to_string();
}

ip_port endpoint_impl::get_port() const {
	return port_;
}

ip_protocol endpoint_impl::get_protocol() const {
	return protocol_;
}

ip_protocol_version endpoint_impl::get_version() const {
	return (address_.is_v4() ?
				ip_protocol_version::V4 : ip_protocol_version::V6);
}

bool endpoint_impl::serialize(serializer *_to) const {
	bool is_successful = _to->serialize(static_cast< uint8_t >(protocol_));
	is_successful = is_successful && _to->serialize(static_cast< uint16_t >(port_));

	if (address_.is_v4()) {
		boost::asio::ip::address_v4 tmp_address = address_.to_v4();
		boost::asio::ip::address_v4::bytes_type bytes = tmp_address.to_bytes();
		is_successful = is_successful && _to->serialize(bytes.data(), bytes.size());
	} else {
		boost::asio::ip::address_v6 tmp_address = address_.to_v6();
		boost::asio::ip::address_v6::bytes_type bytes = tmp_address.to_bytes();
		is_successful = is_successful && _to->serialize(bytes.data(), bytes.size());
	}

	return is_successful;
}

} // namespace vsomeip





