//
// ipv4_multicast_option_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#ifndef VSOMEIP_INTERNAL_SERVICE_DISCOVERY_IPV4_MULTICAST_OPTION_IMPL_HPP
#define VSOMEIP_INTERNAL_SERVICE_DISCOVERY_IPV4_MULTICAST_OPTION_IMPL_HPP

#include <vsomeip/service_discovery/ipv4_multicast_option.hpp>
#include <vsomeip_internal/service_discovery/ipv4_endpoint_option_impl.hpp>

namespace vsomeip {
namespace service_discovery {

class ipv4_multicast_option_impl
		: virtual public ipv4_multicast_option,
		  virtual public ipv4_endpoint_option_impl {
public:
	ipv4_multicast_option_impl();
	virtual ~ipv4_multicast_option_impl();
	bool operator == (const option &_option) const;

	void set_protocol(ip_protocol _protocol);
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SERVICE_DISCOVERY_IPV4_MULTICAST_OPTION_IMPL_HPP
