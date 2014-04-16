//
// timer_service.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_TIMER_SERVICE_HPP
#define VSOMEIP_INTERNAL_SD_TIMER_SERVICE_HPP

#include <boost/asio/system_timer.hpp>

namespace vsomeip {
namespace sd {

struct  timer_service {
	timer_service(boost::asio::io_service &_service);
	~timer_service();

	void start_timer(uint32_t _milliseconds);
	void stop_timer();

	uint32_t expired_from_now();

	virtual void timer_expired(const boost::system::error_code &_error) = 0;
private:

	boost::asio::system_timer timer_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_TIMER_SERVICE_HPP
