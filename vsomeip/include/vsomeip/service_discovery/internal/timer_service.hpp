//
// timer_service.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_INTERNAL_TIMER_SERVICE_HPP
#define VSOMEIP_SERVICE_DISCOVERY_INTERNAL_TIMER_SERVICE_HPP

#include <boost/asio/system_timer.hpp>

namespace vsomeip {
namespace service_discovery {

struct  timer_service {
	timer_service();
	~timer_service();

	void init_timer(boost::asio::io_service &_is);

	void start_timer(uint32_t _milliseconds);
	void stop_timer();

	uint32_t expired_from_now();

	virtual void timer_expired(const boost::system::error_code &_error) = 0;
private:

	boost::asio::system_timer *timer_;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_TIMER_SERVICE_HPP
