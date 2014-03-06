//
// client.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_CLIENT_HPP
#define VSOMEIP_SERVICE_DISCOVERY_CLIENT_HPP

#include <cstddef>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/client_base.hpp>

namespace vsomeip {

class endpoint;

class consumer;
class provider;

namespace service_discovery {

class client
	: virtual public client_base {
public:
	virtual ~client() {};

	virtual consumer * create_consumer(service_id _service, instance_id _instance) = 0;
	virtual provider * create_provider(service_id _service, instance_id _instance,
							   	   	      const endpoint *_source) = 0;
};

} // namespace service_discovery
} // namespace vsomeip



#endif // VSOMEIP_SERVICE_DISCOVERY_CLIENT_HPP
