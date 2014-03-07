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

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint;
class message_base;

class application {
public:
	virtual ~application() {};

	virtual void init() = 0;
	virtual void start() = 0;

	// one of these must be called from the applications event loop
	virtual std::size_t poll_one() = 0;
	virtual std::size_t poll() = 0;
	virtual std::size_t run() = 0;

	// clients
	virtual bool request_service(
						service_id _service, instance_id _instance,
						const endpoint *_location) const = 0;
	virtual bool release_service(
						service_id _service, instance_id _instance) const = 0;

	//virtual bool register_cbk(service_ready_cbk cbk);

	// services
	virtual bool provide_service(
						service_id _service, instance_id _instance,
						const endpoint *_location) const = 0;
	virtual bool start_service(
						service_id _service, instance_id _instance) const = 0;
	virtual bool stop_service(
						service_id _service, instance_id _instance) const = 0;

	virtual bool send(message_base *_message, bool _flush) const = 0;

	virtual void enable_magic_cookies(
						service_id _service, instance_id _instance) const = 0;
	virtual void disable_magic_cookies(
						service_id _service, instance_id _instance) const = 0;

	// watchdog
	virtual void enable_watchdog() = 0;
	virtual void disable_watchdog() = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_APPLICATION_HPP
