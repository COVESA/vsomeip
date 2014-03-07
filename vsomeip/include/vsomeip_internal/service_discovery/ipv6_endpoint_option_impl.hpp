//
// ipv6_endpoint_option_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#ifndef VSOMEIP_INTERNAL_SERVICE_DISCOVERY_IPV6_ENDPOINT_OPTION_IMPL_HPP
#define VSOMEIP_INTERNAL_SERVICE_DISCOVERY_IPV6_ENDPOINT_OPTION_IMPL_HPP

#include <vsomeip/service_discovery/ipv6_endpoint_option.hpp>
#include <vsomeip_internal/service_discovery/option_impl.hpp>

namespace vsomeip {
namespace service_discovery {

class ipv6_endpoint_option_impl
		: virtual public ipv6_endpoint_option,
		  virtual public option_impl {
public:
	ipv6_endpoint_option_impl();
	virtual ~ipv6_endpoint_option_impl();
	bool operator == (const option& _option) const;

	const ipv6_address & get_address() const;
	void set_address(const ipv6_address &_address);

	ip_port get_port() const;
	void set_port(ip_port _port);

	transport_protocol get_protocol() const;
	void set_protocol(transport_protocol _protocol);

	bool serialize(vsomeip::serializer *_to) const;
	bool deserialize(vsomeip::deserializer *_from);

protected:
	ipv6_address address_;
	ip_port port_;
	transport_protocol protocol_;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SERVICE_DISCOVERY_IPV6_ENDPOINT_OPTION_IMPL_HPP
