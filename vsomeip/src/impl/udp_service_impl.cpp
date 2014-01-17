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

udp_service_impl::udp_service_impl(const endpoint &_endpoint)
	: io_(),
	  socket_(io_,
			  ip::udp::endpoint((_endpoint.get_version() == ip_version::V6 ?
			  	  	  	  	  	  	  	  ip::udp::v6() : ip::udp::v4()),
			  _endpoint.get_port())) {
}

udp_service_impl::~udp_service_impl() {
}

void udp_service_impl::start() {
	socket_.async_receive_from(
			boost::asio::buffer(buffer_), remote_,
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

void udp_service_impl::send_callback(boost::system::error_code const &error, std::size_t transferred_bytes) {
}

void udp_service_impl::receive_callback(boost::system::error_code const &error, std::size_t transferred_bytes) {
	if (!error || error == boost::asio::error::message_size) {
		std::cout << "Received(" << std::dec << (int)transferred_bytes << "): ";
		//for (std::size_t i = 0; i < transferred_bytes; ++i)
		//	std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)buffer_[i] << " ";
		std::cout << std::endl;
		start();
	}
}

} // namespace vsomeip
