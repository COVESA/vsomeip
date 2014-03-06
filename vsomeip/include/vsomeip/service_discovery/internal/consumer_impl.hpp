//
// consumer_impl.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CONSUMER_IMPL_HPP
#define VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CONSUMER_IMPL_HPP

#include <boost/variant.hpp>
#include <boost/asio/io_service.hpp>

#include <vsomeip/consumer.hpp>
#include <vsomeip/receiver.hpp>
#include <vsomeip/service_discovery/internal/events.hpp>
#include <vsomeip/internal/statistics_owner_impl.hpp>

namespace vsomeip {
namespace service_discovery {

class consumer_impl
	: virtual public consumer
#ifdef USE_VSOMEIP_STATISTICS
	  , virtual public statistics_owner_impl
#endif
{
public:
	consumer_impl(service_id _service_id, instance_id _instance_id,
			      boost::asio::io_service& _is);
	~consumer_impl();

	void start();
	void stop();
	void register_for(vsomeip::receiver*, vsomeip::service_id, vsomeip::method_id);
	void unregister_for(vsomeip::receiver*, vsomeip::service_id, vsomeip::method_id);
	void enable_magic_cookies();
	void disable_magic_cookies();

	bool send(const vsomeip::message_base*, bool);
	bool send(const uint8_t*, uint32_t, bool);
	bool flush();
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CONSUMER_IMPL_HPP

