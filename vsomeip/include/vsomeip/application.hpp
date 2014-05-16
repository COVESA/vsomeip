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

#include <boost/function.hpp>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint;
class message_base;

typedef boost::function< void (const message_base *) > message_handler_t;

class application {
public:
	virtual ~application() {};

	virtual std::string get_name() const = 0;

	virtual void init(int _options_count, char **_options) = 0;
	virtual void start() = 0;
	virtual void stop() = 0;

	// clients
	virtual bool request_service(
						service_id _service, instance_id _instance,
						const endpoint *_location = 0) = 0;
	virtual bool release_service(
						service_id _service, instance_id _instance) = 0;

	// services
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

	// send & receive
	virtual bool send(message_base *_message, bool _flush = true) = 0;

	virtual bool enable_magic_cookies(service_id _service, instance_id _instance) = 0;
	virtual bool disable_magic_cookies(service_id _service, instance_id _instance) = 0;

	virtual bool register_message_handler(
			service_id _service, instance_id _instance, method_id _method, message_handler_t _handler) = 0;
	virtual void deregister_message_handler(
			service_id _service, instance_id _instance, method_id _method) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_APPLICATION_HPP
