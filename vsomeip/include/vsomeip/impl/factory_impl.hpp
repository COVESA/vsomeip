//
// factory_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_IMPL_FACTORY_IMPL_HPP
#define VSOMEIP_IMPL_FACTORY_IMPL_HPP

#include <map>
#include <vsomeip/factory.hpp>

namespace vsomeip {

class factory_impl : virtual public factory {
public:
	static factory *get_default_factory();
	virtual ~factory_impl();

	message * create_message() const;

	serializer * create_serializer() const;
	deserializer * create_deserializer() const;

	endpoint * create_endpoint(ip_address _address, ip_port _port,
								 ip_protocol _protocol, ip_version _version);
	client * create_client(const endpoint *_endpoint) const;
	service * create_service(const endpoint *_endpoint) const;

private:
	std::map< uint32_t, std::map< std::string, endpoint * > > endpoints_;
};

}; // namespace vsomeip

#endif // VSOMEIP_IMPL_FACTORY_IMPL_HPP
