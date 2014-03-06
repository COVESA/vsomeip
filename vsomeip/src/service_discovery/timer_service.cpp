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
#include <boost/asio/placeholders.hpp>
#include <boost/asio/system_timer.hpp>
#include <boost/bind.hpp>

#include <vsomeip/service_discovery/internal/timer_service.hpp>

namespace vsomeip {
namespace service_discovery {

timer_service::timer_service() {
	timer_ = 0;
}

timer_service::~timer_service() {
	delete timer_;
}

void timer_service::init_timer(boost::asio::io_service &_is) {
	timer_ = new boost::asio::system_timer(_is);
}

void timer_service::start_timer(uint32_t _milliseconds) {
	timer_->expires_from_now(
			std::chrono::milliseconds(_milliseconds));
	timer_->async_wait(boost::bind(
			&timer_service::timer_expired,
			this,
			boost::asio::placeholders::error));
}

void timer_service::stop_timer() {
	timer_->cancel();
}

uint32_t timer_service::expired_from_now() {
	return std::chrono::duration_cast<
				std::chrono::milliseconds>(
					timer_->expires_from_now()).count();
}

} // namespace service_discovery
} // namespace vsomeip
