//
// factory.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_FACTORY_HPP
#define VSOMEIP_SERVICE_DISCOVERY_FACTORY_HPP

#include <vsomeip/factory.hpp>

namespace vsomeip {
namespace service_discovery {

class client;
class message;

class factory
		: virtual public vsomeip::factory {
public:
	static factory * get_default_factory();
	virtual ~factory() {};

	virtual client * create_service_discovery_client() const = 0;
	virtual message * create_service_discovery_message() const = 0;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_FACTORY_HPP

