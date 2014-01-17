//
// udp_service_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <iomanip>
#include <iostream>

#include <boost/bind.hpp>

#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/endpoint.hpp>
#include <vsomeip/impl/udp_service_impl.hpp>

namespace ip = boost::asio::ip;

namespace vsomeip {

udp_service_impl::udp_service_impl(const endpoint &_endpoint) :
		io_(), socket_(io_,
				ip::udp::endpoint(
						(_endpoint.get_version() == ip_version::V6 ?
								ip::udp::v6() : ip::udp::v4()),
						_endpoint.get_port())) {
}

udp_service_impl::~udp_service_impl() {
}

void udp_service_impl::start() {
	socket_.async_receive_from(boost::asio::buffer(buffer_), remote_,
			boost::bind(&udp_service_impl::receive_callback, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
}

void udp_service_impl::stop() {
}

std::size_t udp_service_impl::poll_one() {
	return io_.poll_one();
}

std::size_t udp_service_impl::poll() {
	return io_.poll();
}

std::size_t udp_service_impl::run() {
	return io_.run();
}

void udp_service_impl::send_callback(boost::system::error_code const &error,
		std::size_t _sent_bytes) {
#ifdef USE_VSOMEIP_STATISTICS
	statistics_.sent_messages_++;
	statistics_.sent_bytes_ += _sent_bytes;
#endif
}

void udp_service_impl::receive_callback(boost::system::error_code const &error,
		std::size_t _sent_bytes) {
	if (!error || error == boost::asio::error::message_size) {
#ifdef USE_VSOMEIP_STATISTICS
		statistics_.received_messages_++;
		statistics_.received_bytes_ += _sent_bytes;
		if (statistics_.received_messages_ % 10000 == 0)
			std::cout << statistics_.get_received_messages_count() << " "
					  << statistics_.get_received_bytes_count() << std::endl;
#endif
		start();
	} else {
		std::cerr << "Error " << error.message() << std::endl;
	}
}

}
 // namespace vsomeip
