//
// client_manager.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_CLIENT_MANAGER_HPP
#define VSOMEIP_INTERNAL_SD_CLIENT_MANAGER_HPP

#include <boost/asio/io_service.hpp>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class application;
class message_base;

namespace sd {

class client_manager {
public:
	virtual ~client_manager() {};

	virtual application * get_owner() const = 0;
	virtual void set_owner(application *_owner) = 0;

	virtual boost::asio::io_service & get_service() = 0;

	virtual void init() = 0;
	virtual void start() = 0;
	virtual void stop() = 0;

	virtual void on_provide_service(service_id _service, instance_id _instance) = 0;
	virtual void on_withdraw_service(service_id _service, instance_id _instance) = 0;

	virtual void on_start_service(service_id _service, instance_id _instance) = 0;
	virtual void on_stop_service(service_id _service, instance_id _instance) = 0;

	virtual void on_request_service(service_id _service, instance_id _instance) = 0;
	virtual void on_release_service(service_id _service, instance_id _instance) = 0;
};

} // sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_CLIENT_MANAGER_HPP
