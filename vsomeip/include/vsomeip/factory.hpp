//
// factory.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_FACTORY_HPP
#define VSOMEIP_FACTORY_HPP

#include <string>

#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class application;
class endpoint;
class message_base;
class message;

class factory {
public:
	virtual ~factory() {};

	static factory * get_instance();

	virtual application * create_application(const std::string &_name) const = 0;
	virtual application * create_managing_application(const std::string &_name) const = 0;

	virtual endpoint * get_endpoint(ip_address _address, ip_port _port, ip_protocol _protocol) = 0;
	virtual endpoint * get_endpoint(const uint8_t *_bytes, uint32_t _size) = 0;

	virtual message * create_message() const = 0;
	virtual message * create_response(const message_base  *_request) const = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_FACTORY_HPP
