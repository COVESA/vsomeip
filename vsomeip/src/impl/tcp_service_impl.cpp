//
// tcp_service_impl.cpp
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
#include <vsomeip/impl/tcp_service_impl.hpp>

namespace ip = boost::asio::ip;

namespace vsomeip {

tcp_service_impl::tcp_service_impl(const endpoint &_endpoint)
: io_(), acceptor_(io_,
		ip::tcp::endpoint((
				_endpoint.get_version() == ip_version::V6 ?
				ip::tcp::v6() : ip::tcp::v4()),
		_endpoint.get_port())) {
}

tcp_service_impl::~tcp_service_impl() {
}

void tcp_service_impl::start() {
	connection::pointer new_connection = connection::create(io_);

	acceptor_.async_accept(new_connection->get_socket(),
			boost::bind(&tcp_service_impl::accept_callback, this,
					new_connection, boost::asio::placeholders::error));
}

void tcp_service_impl::stop() {
}

std::size_t tcp_service_impl::poll_one() {
	return io_.poll_one();
}

std::size_t tcp_service_impl::poll() {
	return io_.poll();
}

std::size_t tcp_service_impl::run() {
	return io_.run();
}

void tcp_service_impl::accept_callback(connection::pointer new_connection,
		const boost::system::error_code& error) {
	if (!error) {
		new_connection->start();
	}

	start();
}

// Inner class connection
tcp_service_impl::connection::connection(boost::asio::io_service &_io)
	: socket_(_io) {
}

tcp_service_impl::connection::pointer tcp_service_impl::connection::create(
		boost::asio::io_service &_io) {
	return pointer(new connection(_io));
}

boost::asio::ip::tcp::socket & tcp_service_impl::connection::get_socket() {
	return socket_;
}

void tcp_service_impl::connection::start() {
	socket_.async_read_some(boost::asio::buffer(buffer_),
			boost::bind(&connection::receive_callback, shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
}

void tcp_service_impl::connection::send_callback(
		boost::system::error_code const &error, std::size_t transferred_bytes) {
}

void tcp_service_impl::connection::receive_callback(
		boost::system::error_code const &error, std::size_t transferred_bytes) {
	std::cout << "Received(" << (int)transferred_bytes << ")" << std::endl;
	if (!error) start();
}

} // namespace vsomeip
