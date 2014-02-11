//
// registry.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_INTERNAL_REGISTRY_HPP
#define	 VSOMEIP_SERVICE_DISCOVERY_INTERNAL_REGISTRY_HPP

#include <map>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {
namespace service_discovery {

class registry {
public:
	class service {
	public:
		service_id service_id_;
		instance_id instance_id_;
	};

	service * add(service_id _service_id, instance_id _instance_id);
	service * search(service_id _service_id, instance_id _instance_id);

private:
	std::map< service_id,
			  std::map< instance_id,
			  	  	    service > > data_;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_REGISTRY_HPP
