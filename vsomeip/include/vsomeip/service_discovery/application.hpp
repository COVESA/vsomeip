//
// application_impl.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_APPLICATION_HPP
#define VSOMEIP_SERVICE_DISCOVERY_APPLICATION_HPP

#include <cstddef>

#include <vsomeip/application.hpp>
#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class client;
class endpoint;

namespace service_discovery {

class service;

class application
	: virtual public vsomeip::application {
public:
	virtual ~application() {};

	virtual client * create_service_discovery_client(service_id _service, instance_id _instance) = 0;
	virtual service * create_service_discovery_service(service_id _service, instance_id _instance,
							   	   	    const endpoint *_source) = 0;
};

} // namespace service_discovery
} // namespace vsomeip



#endif // VSOMEIP_SERVICE_DISCOVERY_APPLICATION_HPP
