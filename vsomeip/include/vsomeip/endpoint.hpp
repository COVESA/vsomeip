//
// endpoint.hpp
//
// Date: 	Jan 13, 2014
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_ENDPOINT_HPP
#define VSOMEIP_ENDPOINT_HPP

#include <string>
#include <vsomeip/enumeration_types.hpp>

namespace vsomeip {

class endpoint {
public:
	virtual ~endpoint() {};

	virtual std::string get_address() const = 0;
	virtual void set_address(const std::string &_address) = 0;

	virtual uint16_t get_port() const = 0;
	virtual void set_port(const uint16_t _port) = 0;

	virtual ip_protocol get_protocol() const = 0;
	virtual void set_protocol(ip_protocol _protocol) = 0;

	virtual ip_version get_version() const = 0;
	virtual void set_version(ip_version _version) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_ENDPOINT_HPP
