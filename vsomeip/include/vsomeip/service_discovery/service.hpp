//
// service.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_SERVICE_HPP
#define VSOMEIP_SERVICE_DISCOVERY_SERVICE_HPP

#include <vsomeip/service.hpp>

namespace vsomeip {
namespace service_discovery {

class service : virtual public vsomeip::service {
public:
	virtual bool register_service(service_id _service, instance_id _instance) = 0;
	virtual bool unregister_service(service_id _service, instance_id _instance) = 0;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_SERVICE_HPP
