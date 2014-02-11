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
#ifndef VSOMEIP_SERVICE_DISCOVERY_INTERNAL_FACTORYIMPL_HPP
#define VSOMEIP_SERVICE_DISCOVERY_INTERNAL_FACTORYIMPL_HPP

#include <vsomeip/internal/factory_impl.hpp>
#include <vsomeip/service_discovery/factory.hpp>

namespace vsomeip {
namespace service_discovery {

class factory_impl
		: virtual public factory,
		  virtual public vsomeip::factory_impl {
public:
	static vsomeip::service_discovery::factory * get_default_factory();
	virtual ~factory_impl();

	message * create_service_discovery_message() const;
	deserializer * create_deserializer() const;

	client * create_client(const endpoint *_target) const;
	client * create_client(service_id _service_id,
							 instance_id _instance_id,
							 major_version _major_version,
							 time_to_live _time_to_live) const;

	service * create_service(const endpoint *_endpoint) const;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_FACTORYIMPL_HPP
