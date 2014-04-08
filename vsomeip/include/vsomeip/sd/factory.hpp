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

#ifndef VSOMEIP_SD_FACTORY_HPP
#define VSOMEIP_SD_FACTORY_HPP

#include <vsomeip/factory.hpp>

namespace vsomeip {
namespace sd {

class message;

class factory : virtual public vsomeip::factory {
public:
	virtual ~factory() {};

	static factory * get_instance();

	virtual message * create_service_discovery_message() const = 0;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_FACTORY_HPP
