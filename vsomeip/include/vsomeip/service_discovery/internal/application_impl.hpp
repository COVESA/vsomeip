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

#ifndef VSOMEIP_SERVICE_DISCOVERY_INTERNAL_APPLICATION_IMPL_HPP
#define VSOMEIP_SERVICE_DISCOVERY_INTERNAL_APPLICATION_IMPL_HPP

#include <boost/asio/io_service.hpp>

#include <vsomeip/service_discovery/application.hpp>
#include <vsomeip/internal/application_impl.hpp>

namespace vsomeip {
namespace service_discovery {

class application_impl
	: virtual public application,
	  virtual public vsomeip::application_impl {
public:
	virtual ~application_impl();

	client * create_service_discovery_client(service_id _service, instance_id _instance);
	service * create_service_discovery_service(service_id _service, instance_id _instance,
							   const endpoint *_source);

private:
	std::map< uint32_t, client *> service_discovery_clients_;
};

} // namespace service_discovery
} // namespace vsomeip



#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_APPLICATION_IMPL_HPP
