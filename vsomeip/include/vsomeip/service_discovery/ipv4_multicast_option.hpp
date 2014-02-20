//
// ipv4_multicast_option.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_IPV4_MULTICAST_OPTION_HPP
#define VSOMEIP_SERVICE_DISCOVERY_IPV4_MULTICAST_OPTION_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/service_discovery/ipv4_endpoint_option.hpp>

namespace vsomeip {
namespace service_discovery {

class ipv4_multicast_option
		: virtual public ipv4_endpoint_option {
public:
	virtual ~ipv4_multicast_option() {};
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_IPV4_MULTICAST_OPTION_HPP
