//
// factory.hpp
//
// Date: 	Nov 18, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_FACTORY_HPP
#define VSOMEIP_FACTORY_HPP

#include <vsomeip/message.hpp>
#include <vsomeip/serializer.hpp>
#include <vsomeip/deserializer.hpp>

namespace vsomeip {

class message;

class factory {
public:
	static factory * get_default_factory();
	virtual ~factory() {};

	virtual message * create_message() const = 0;
	virtual serializer * create_serializer() const = 0;
	virtual deserializer * create_deserializer(uint8_t *_data = 0, uint32_t _length = 0) const = 0;
};

}; // namespace vsomeip

#endif // VSOMEIP_FACTORY_HPP
