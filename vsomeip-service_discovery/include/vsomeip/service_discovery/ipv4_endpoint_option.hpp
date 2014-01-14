//
// ip4endpoint_option.hpp
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_IP4ENDPOINT_OPTION_HPP
#define VSOMEIP_SERVICE_DISCOVERY_IP4ENDPOINT_OPTION_HPP

#include <vsomeip/service_discovery/option.hpp>
#include <vsomeip/service_discovery/primitive_types.hpp>

namespace vsomeip {
namespace service_discovery {

class ipv4_endpoint_option
		: virtual public option {
public:
	virtual ~ipv4_endpoint_option() {};

	virtual ipv4_address get_address() const = 0;
	virtual void set_address(ipv4_address _address) = 0;

	virtual ip_port get_port() const = 0;
	virtual void set_port(ip_port _port) = 0;

	virtual ip_protocol get_protocol() const = 0;
	virtual void set_protocol(ip_protocol _protocol) = 0;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_IP4ENDPOINT_OPTION_HPP
