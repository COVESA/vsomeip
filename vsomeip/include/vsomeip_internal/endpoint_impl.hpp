//
// endpoint_impl.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_ENDPOINT_IMPL_HPP
#define VSOMEIP_INTERNAL_ENDPOINT_IMPL_HPP

#include <boost/asio/ip/address.hpp>

#include <vsomeip/endpoint.hpp>

namespace vsomeip {

class endpoint_impl
	: virtual public endpoint {
public:
	endpoint_impl(boost::asio::ip::address _address, ip_port _port, ip_protocol _protocol);
	virtual ~endpoint_impl();

	ip_address get_address() const;
	ip_port get_port() const;
	ip_protocol get_protocol() const;
	ip_protocol_version get_version() const;

	bool serialize(serializer *_to) const;

private:
	boost::asio::ip::address address_;
	uint16_t port_;
	ip_protocol protocol_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_ENDPOINT_IMPL_HPP
