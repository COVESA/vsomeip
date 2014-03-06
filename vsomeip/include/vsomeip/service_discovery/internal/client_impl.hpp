//
// client_impl.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CLIENT_IMPL_HPP
#define VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CLIENT_IMPL_HPP

#include <map>

#include <vsomeip/internal/client_base_impl.hpp>
#include <vsomeip/service_discovery/client.hpp>

namespace vsomeip {
namespace service_discovery {

class client_impl
	: virtual public client,
	  virtual public client_base_impl {
public:
	virtual ~client_impl();

	consumer * create_consumer(service_id _service, instance_id _instance);
	provider * create_provider(service_id _service, instance_id _instance,
							   	 const endpoint *_source);

private:
	provider * create_provider(const endpoint *_source);

	typedef uint32_t service_instance;
	std::map<service_instance, consumer *> consumers_;
	std::map<const endpoint *, provider *> providers_;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CLIENT_IMPL_HPP
