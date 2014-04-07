//
// ipv4_multicast_option.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SD_IPV4_MULTICAST_OPTION_HPP
#define VSOMEIP_SD_IPV4_MULTICAST_OPTION_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/sd/ipv4_endpoint_option.hpp>

namespace vsomeip {
namespace sd {

class ipv4_multicast_option
		: virtual public ipv4_endpoint_option {
public:
	virtual ~ipv4_multicast_option() {};
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_IPV4_MULTICAST_OPTION_HPP
