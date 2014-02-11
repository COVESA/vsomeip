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

class client;
class service;

namespace service_discovery {

class message;

class factory
		: virtual public vsomeip::factory {
public:
	static factory * get_default_factory();
	virtual ~factory() {};

	virtual message * create_service_discovery_message() const = 0;
	virtual deserializer * create_deserializer() const = 0;

	virtual client * create_client(
						const endpoint *_target) const = 0;

	virtual client * create_client(
						service_id _service_id,
						instance_id _instance_id = 0xFFFF,
						major_version _major_version = 0x0,
						time_to_live _time_to_live = 0xFFFFFF) const = 0;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_FACTORY_HPP

