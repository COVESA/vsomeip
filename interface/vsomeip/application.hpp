// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_APPLICATION_HPP
#define VSOMEIP_APPLICATION_HPP

#include <vector>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/constants.hpp>
#include <vsomeip/handler.hpp>

namespace vsomeip {

class event;
class payload;

class application {
public:
	virtual ~application() {};

	// Lifecycle
	virtual bool init() = 0;
	virtual void start() = 0;
	virtual void stop() = 0;

	// Provide services
	virtual void offer_service(service_t _service, instance_t _instance,
					major_version_t _major = VSOMEIP_DEFAULT_MAJOR,
					minor_version_t _minor = VSOMEIP_DEFAULT_MINOR, ttl_t _ttl =
					VSOMEIP_DEFAULT_TTL) = 0;

	virtual void stop_offer_service(service_t _service,
					instance_t _instance) = 0;

	virtual void publish_eventgroup(service_t _service, instance_t _instance,
					eventgroup_t _eventgroup, major_version_t _major,
					ttl_t _ttl) = 0;

	virtual void stop_publish_eventgroup(service_t _service,
					instance_t _instance, eventgroup_t _eventgroup) = 0;

	// Consume services
	virtual void request_service(service_t _service, instance_t _instance,
					major_version_t _major = VSOMEIP_ANY_MAJOR,
					minor_version_t _minor = VSOMEIP_ANY_MINOR,
					ttl_t _ttl = VSOMEIP_ANY_TTL) = 0;

	virtual void release_service(service_t _service, instance_t _instance) = 0;

	virtual void subscribe(service_t _service, instance_t _instance,
					eventgroup_t _eventgroup) = 0;

	virtual void unsubscribe(service_t _service, instance_t _instance,
					eventgroup_t _eventgroup) = 0;

	virtual bool is_available(service_t _service, instance_t _instance) = 0;

	// Define content of eventgroups
	virtual std::shared_ptr< event > add_event(service_t _service,
				instance_t _instance, eventgroup_t _eventgroup, event_t _event) = 0;

	virtual std::shared_ptr< event > add_field(service_t _service,
				instance_t _instance, eventgroup_t _eventgroup, event_t _event,
				std::shared_ptr< payload > _payload) = 0;

	virtual void remove_event_or_field(
					std::shared_ptr< event > _event /* or field */) = 0;

	// Send messages
	virtual void send(std::shared_ptr< message > _message,
					bool _flush = true,	bool _reliable = false) = 0;

	// Receive messages
	virtual bool register_message_handler(service_t _service,
					instance_t _instance, method_t _method,
					message_handler_t _handler) = 0;
	virtual bool unregister_message_handler(service_t _service,
					instance_t _instance, method_t _method) = 0;

	// Receive availability
	virtual bool register_availability_handler(service_t _service,
					instance_t _instance, availability_handler_t _handler) = 0;
	virtual bool unregister_availability_handler(service_t _service,
					instance_t _instance) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_APPLICATION_HPP
