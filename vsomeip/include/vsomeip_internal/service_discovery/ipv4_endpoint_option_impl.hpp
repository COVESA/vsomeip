//
// ipv4_endpoint_option_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#ifndef VSOMEIP_INTERNAL_SERVICE_DISCOVERY_IPV4_ENDPOINT_OPTION_IMPL_HPP
#define VSOMEIP_INTERNAL_SERVICE_DISCOVERY_IPV4_ENDPOINT_OPTION_IMPL_HPP

#include <vsomeip/service_discovery/ipv4_endpoint_option.hpp>
#include <vsomeip_internal/service_discovery/option_impl.hpp>

namespace vsomeip {
namespace service_discovery {

class ipv4_endpoint_option_impl
		: virtual public ipv4_endpoint_option,
		  virtual public option_impl {
public:
	ipv4_endpoint_option_impl();
	virtual ~ipv4_endpoint_option_impl();
	bool operator == (const option &_option) const;

	ipv4_address get_address() const;
	void set_address(ipv4_address _address);

	ip_port get_port() const;
	void set_port(ip_port _port);

	ip_protocol get_protocol() const;
	void set_protocol(ip_protocol _protocol);

	bool serialize(vsomeip::serializer *_to) const;
	bool deserialize(vsomeip::deserializer *_from);

protected:
	ipv4_address address_;
	ip_port port_;
	ip_protocol protocol_;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SERVICE_DISCOVERY_IPV4_ENDPOINT_OPTION_IMPL_HPP
