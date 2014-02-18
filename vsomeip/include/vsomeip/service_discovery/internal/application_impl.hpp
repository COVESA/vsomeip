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

#include <boost/asio/io_service.hpp>

#include <vsomeip/service_discovery/application.hpp>

#ifndef VSOMEIP_SERVICE_DISCOVERY_INTERNAL_APPLICATION_IMPL_HPP
#define VSOMEIP_SERVICE_DISCOVERY_INTERNAL_APPLICATION_IMPL_HPP

namespace vsomeip {
namespace service_discovery {

class application_impl
	: public application {
public:
	virtual ~application_impl();

	client * create_service_discovery_client(service_id _service, instance_id _instance);
	service * create_service_discovery_service(service_id _service, instance_id _instance,
							   const endpoint *_source);

	std::size_t poll_one();
	std::size_t poll();
	std::size_t run();

private:
	boost::asio::io_service is_;
};

} // namespace service_discovery
} // namespace vsomeip



#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_APPLICATION_IMPL_HPP
