//
// service_impl.hpp
//
// Date: 	Jan 14, 2014
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_IMPL_SERVICE_IMPL_HPP
#define VSOMEIP_IMPL_SERVICE_IMPL_HPP

#include <vsomeip/service.hpp>

namespace vsomeip {

class service_impl : virtual public service {
public:
	service_impl();
	service_impl(const service_impl &_impl);
	virtual ~service_impl();

	std::string get_address() const;
	void set_address(const std::string &_address);

	uint16_t get_port() const;
	void set_port(uint16_t _port);

	ip_protocol get_protocol() const;
	void set_protocol(ip_protocol _protocol);

protected:
	std::string address_;
	uint16_t port_;

	ip_protocol protocol_;
};

} // namespace vsomeip

#endif // VSOMEIP_IMPL_SERVICE_IMPL_HPP
