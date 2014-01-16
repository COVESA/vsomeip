//
// factory_impl.hpp
//
// Date: 	Nov 18, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_IMPL_FACTORY_IMPL_HPP
#define VSOMEIP_IMPL_FACTORY_IMPL_HPP

#include <vsomeip/factory.hpp>

namespace vsomeip {

class factory_impl : virtual public factory {
public:
	static factory *get_default_factory();
	virtual ~factory_impl();

	message * create_message() const;

	serializer * create_serializer() const;
	deserializer * create_deserializer(uint8_t *_data, uint32_t _length) const;

	endpoint * create_endpoint() const;
	client * create_client(const endpoint &_endpoint) const;
	service * create_service(const endpoint &_endpoint) const;
};

}; // namespace vsomeip

#endif // VSOMEIP_IMPL_FACTORY_IMPL_HPP
