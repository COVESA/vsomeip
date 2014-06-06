//
// service_discovery.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_SERVICE_DISCOVERY_HPP
#define VSOMEIP_INTERNAL_SD_SERVICE_DISCOVERY_HPP

#include <boost/asio/io_service.hpp>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class daemon;
class endpoint;
class message_base;
class message;

namespace sd {

class service_discovery {
public:
	virtual ~service_discovery() {};

	virtual boost::asio::io_service & get_service() = 0;

	virtual void init() = 0;
	virtual void start() = 0;
	virtual void stop() = 0;

	virtual void on_provide_service(service_id _service, instance_id _instance, const endpoint *_location) = 0;
	virtual void on_withdraw_service(service_id _service, instance_id _instance, const endpoint *_location) = 0;
	virtual void on_start_service(service_id _service, instance_id _instance) = 0;
	virtual void on_stop_service(service_id _service, instance_id _instance) = 0;

	virtual void on_request_service(client_id _client, service_id _service, instance_id _instance) = 0;
	virtual void on_release_service(client_id _client, service_id _service, instance_id _instance) = 0;

	virtual void on_provide_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, const endpoint *_multicast) = 0;
	virtual void on_withdraw_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, const endpoint *_multicast) = 0;

	virtual void on_request_eventgroup(client_id _client, service_id _service, instance_id _instance, eventgroup_id _eventgroup) = 0;
	virtual void on_release_eventgroup(client_id _client, service_id _service, instance_id _instance, eventgroup_id _eventgroup) = 0;

	virtual void on_message(const uint8_t *_data, uint32_t _size, const endpoint *_source, const endpoint *_target) = 0;

	virtual void on_service_available(service_id _service, instance_id _instance, const endpoint *_reliable, const endpoint *_unreliable) = 0;

	virtual bool send(message *_message) = 0;

	virtual bool update_client(client_id _client,
							   service_id _service, instance_id _instance, const endpoint *_location,
							   bool _is_available) = 0;
};

} // sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_SERVICE_DISCOVERY_HPP
