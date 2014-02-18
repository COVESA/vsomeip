//
// service_impl.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_INTERNAL_SERVICE_IMPL_HPP
#define VSOMEIP_SERVICE_DISCOVERY_INTERNAL_SERVICE_IMPL_HPP

#include <boost/variant.hpp>
#include <boost/asio/io_service.hpp>

#include <vsomeip/service_discovery/service.hpp>
#include <vsomeip/service_discovery/internal/events.hpp>
#include <vsomeip/internal/statistics_owner_impl.hpp>

namespace vsomeip {
namespace service_discovery {

class service_impl
	: virtual public service
#ifdef USE_VSOMEIP_STATISTICS
	  , virtual public statistics_owner_impl
#endif
{
public:
	service_impl(vsomeip::service *_delegate);
	virtual ~service_impl();

	bool register_service(service_id _service, instance_id _instance);
	bool unregister_service(service_id _service, instance_id _instance);

	void start();
	void stop();

	void register_for(receiver *_receiver,
				 	 	service_id _service_id,
				 	 	method_id _method_id);

	void unregister_for(receiver *_receiver,
	 	 				  service_id _service_id,
	 	 				  method_id _method_id);

	void enable_magic_cookies();
	void disable_magic_cookies();



	bool send(const message_base *_message, bool _flush);
	bool send(const uint8_t *_data, uint32_t _length, endpoint *_target, bool _flush);
	bool flush(endpoint *_target);

private:
	vsomeip::service *delegate_;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_SERVICE_IMPL_HPP

