//
// application.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_APPLICATION_HPP
#define VSOMEIP_APPLICATION_HPP

#include <functional>
#include <memory>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint;
class field;
class message;

typedef std::function< void (std::shared_ptr< const message > &) > message_handler_t;
typedef std::function< void (service_id, instance_id, bool) > availability_handler_t;

class application {
public:
	virtual ~application() {};

	virtual std::string get_name() const = 0;

	virtual void init(int _options_count, char **_options) = 0;
	virtual void start() = 0;
	virtual void stop() = 0;

	// services (service side)
	virtual bool provide_service(
						service_id _service, instance_id _instance,
						const endpoint *_location) = 0;
	virtual bool withdraw_service(
						service_id _service, instance_id _instance,
						const endpoint *_location = 0) = 0;

	virtual bool start_service(
						service_id _service, instance_id _instance) = 0;
	virtual bool stop_service(
						service_id _service, instance_id _instance) = 0;

	// services (client side)
	virtual bool request_service(
						service_id _service, instance_id _instance,
						const endpoint *_location = 0) = 0;
	virtual bool release_service(
						service_id _service, instance_id _instance) = 0;

	virtual bool is_service_available(service_id _service, instance_id _instance) const = 0;
	virtual bool register_availability_handler(service_id _service, instance_id _instance,
											   availability_handler_t _handler) = 0;
	virtual void deregister_availability_handler(service_id _service, instance_id _instance) = 0;

	// eventgroups (service side)
	virtual bool provide_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup,
									const endpoint *_location) = 0;
	virtual bool withdraw_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup,
									 const endpoint *_location) = 0;

	virtual bool add_field(service_id _service, instance_id _instance, eventgroup_id _eventgroup, field *_field) = 0;
	virtual bool remove_field(service_id _service, instance_id _instance, eventgroup_id _eventgroup, field *_field) = 0;
	virtual bool update_field(const field *_field) = 0;

	// eventgroups (client side)
	virtual bool request_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup) = 0;
	virtual bool release_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup) = 0;

	// send & receive
	virtual bool send(message *_message, bool _reliable = true, bool _flush = true) = 0;

	virtual bool enable_magic_cookies(service_id _service, instance_id _instance) = 0;
	virtual bool disable_magic_cookies(service_id _service, instance_id _instance) = 0;

	virtual bool register_message_handler(
			service_id _service, instance_id _instance, method_id _method, message_handler_t _handler) = 0;
	virtual void deregister_message_handler(
			service_id _service, instance_id _instance, method_id _method) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_APPLICATION_HPP
