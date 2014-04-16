//
// timer_service.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <boost/asio/placeholders.hpp>
#include <boost/asio/system_timer.hpp>
#include <boost/bind.hpp>

#include <vsomeip_internal/sd/timer_service.hpp>

namespace vsomeip {
namespace sd {

timer_service::timer_service(boost::asio::io_service &_service)
	: timer_(_service) {
}

timer_service::~timer_service() {
}

void timer_service::start_timer(uint32_t _milliseconds) {
	timer_.expires_from_now(std::chrono::milliseconds(_milliseconds));
	timer_.async_wait(
		boost::bind(
			&timer_service::timer_expired,
			this,
			boost::asio::placeholders::error
		)
	);
}

void timer_service::stop_timer() {
	timer_.cancel();
}

uint32_t timer_service::expired_from_now() {
	return std::chrono::duration_cast<
				std::chrono::milliseconds>(
					timer_.expires_from_now()).count();
}

} // namespace sd
} // namespace vsomeip
