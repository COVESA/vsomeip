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

#include <vsomeip/endpoint.hpp>

namespace vsomeip {

class endpoint_impl : virtual public endpoint {
public:
	endpoint_impl(ip_address _address, ip_port _port,
					transport_protocol _protocol, transport_protocol_version _version);
	virtual ~endpoint_impl();

private:
	ip_address get_address() const;
	ip_port get_port() const;
	transport_protocol get_protocol() const;
	transport_protocol_version get_version() const;

private:
	ip_address address_;
	ip_port port_;
	transport_protocol protocol_;
	transport_protocol_version version_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_ENDPOINT_IMPL_HPP
