//
// udp_client_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright 2013, 2024 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <boost/asio/placeholders.hpp>
#include <boost/bind.hpp>

#include <vsomeip_internal/udp_client_impl.hpp>

namespace vsomeip {

udp_client_impl::udp_client_impl(
		managing_application_impl *_owner, const endpoint *_location)
	: udp_client_base_impl(_owner, _location) {
}

udp_client_impl::~udp_client_impl() {
}

void udp_client_impl::connect() {
	socket_.async_connect(
		local_,
		boost::bind(
			&udp_client_base_impl::connect_cbk,
			this,
			boost::asio::placeholders::error
		)
	);
}

void udp_client_impl::start() {
	socket_.open(local_.protocol().v4());
	connect();
	receive();
}

void udp_client_impl::send_queued() {
	socket_.async_send(
		boost::asio::buffer(
			&packet_queue_.front()[0],
			packet_queue_.front().size()
		),
		boost::bind(
			&udp_client_base_impl::send_cbk,
			this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred
		)
	);
}

void udp_client_impl::receive() {
	socket_.async_receive_from(
		boost::asio::buffer(buffer_),
		remote_,
		boost::bind(
			&udp_client_base_impl::receive_cbk,
			this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred
		)
	);
}

ip_address udp_client_impl::get_remote_address() const {
	return remote_.address().to_string();
}

ip_port udp_client_impl::get_remote_port() const {
	return remote_.port();
}

ip_protocol udp_client_impl::get_protocol() const {
	return ip_protocol::UDP;
}

} // namespace vsomeip

