//
// proxy.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_PROXY_HPP
#define VSOMEIP_INTERNAL_PROXY_HPP

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint;
class message_base;

class proxy {
public:
	virtual ~proxy() {};

	virtual void init() = 0;
	virtual void start() = 0;
	virtual void stop() = 0;

	virtual bool provide_service(service_id _service, instance_id _instance, const endpoint *_location) = 0;
	virtual bool withdraw_service(service_id _service, instance_id _instance, const endpoint *_location) = 0;

	virtual bool start_service(service_id _service, instance_id _instance) = 0;
	virtual bool stop_service(service_id _service, instance_id _instance) = 0;

	virtual bool request_service(service_id _service, instance_id _instance, const endpoint *_location) = 0;
	virtual bool release_service(service_id _servive, instance_id _instance) = 0;

	virtual bool provide_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, const endpoint *_location) = 0;
	virtual bool withdraw_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, const endpoint *_location) = 0;

	virtual bool add_to_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, event_id _event) = 0;
	virtual bool add_to_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, message_base *_field) = 0;
	virtual bool remove_from_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, event_id _event) = 0;

	virtual bool request_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup) = 0;
	virtual bool release_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup) = 0;

	virtual void register_method(service_id _service, instance_id _instance, method_id _method) = 0;
	virtual void deregister_method(service_id _service, instance_id _instance, method_id _method) = 0;

	virtual bool send(message_base *_message, bool _flush) = 0;

	virtual bool enable_magic_cookies(service_id _service, instance_id _instance) = 0;
	virtual bool disable_magic_cookies(service_id _service, instance_id _instance) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_PROXY_HPP
