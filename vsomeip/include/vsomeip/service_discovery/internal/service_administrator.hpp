//
// service_administrator.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_INTERNAL_SERVICE_ADMINISTRATOR_HPP
#define VSOMEIP_SERVICE_DISCOVERY_INTERNAL_SERVICE_ADMINISTRATOR_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/service_discovery/internal/service_administrator_behavior.hpp>

namespace vsomeip {
namespace service_discovery {

class daemon;

class service_administrator
	: public service_administrator_behavior {
public:
	service_administrator(
		daemon *_daemon, service_id _service, instance_id _instance);

	void init();
	void start();
	void stop();

	void send_offer_service(endpoint *_target = 0);
	void send_stop_offer_service();

	void timer_expired(const boost::system::error_code&);

private:
	daemon *daemon_;
	service_id service_;
	instance_id instance_;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_SERVICE_ADMINISTRATOR_HPP
