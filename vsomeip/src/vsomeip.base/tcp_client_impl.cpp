//
// tcp_client_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright 2013, 2024 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <boost/asio/placeholders.hpp>
#include <boost/asio/write.hpp>
#include <boost/bind.hpp>

#include <vsomeip_internal/tcp_client_impl.hpp>

namespace ip = boost::asio::ip;

namespace vsomeip {

tcp_client_impl::tcp_client_impl(
		managing_application *_owner, const endpoint *_location)
	: tcp_client_base_impl(_owner, _location) {
	is_supporting_magic_cookies_ = true;
}

tcp_client_impl::~tcp_client_impl() {
}

void tcp_client_impl::start() {
	socket_.open(local_.protocol());
	connect();

	// Nagle algorithm off
	ip::tcp::no_delay option;
	socket_.set_option(option);

	receive();
}

void tcp_client_impl::connect() {
	socket_.async_connect(
		local_,
		boost::bind(
			&tcp_client_base_impl::connect_cbk,
			this,
			boost::asio::placeholders::error
		)
	);
}

void tcp_client_impl::receive() {
	socket_.async_receive(
		boost::asio::buffer(buffer_),
		boost::bind(
			&tcp_client_base_impl::receive_cbk,
			this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred
		)
	);
}

void tcp_client_impl::send_queued() {
	if (has_enabled_magic_cookies_)
		send_magic_cookie();

	boost::asio::async_write(
		socket_,
		boost::asio::buffer(
			&packet_queue_.front()[0],
			packet_queue_.front().size()
		),
		boost::bind(
			&tcp_client_base_impl::send_cbk,
			this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred
		)
	);
}

ip_address tcp_client_impl::get_remote_address() const {
	return local_.address().to_string();
}

ip_port tcp_client_impl::get_remote_port() const {
	return local_.port();
}

ip_protocol tcp_client_impl::get_protocol() const {
	return ip_protocol::TCP;
}

void tcp_client_impl::send_magic_cookie() {
	static uint8_t data[] = { 0xFF, 0xFF, 0x00, 0x00,
							  0x00, 0x00, 0x00, 0x08,
							  0xDE, 0xAD, 0xBE, 0xEF,
							  0x01, 0x01, 0x01, 0x00 };

	std::vector<uint8_t>& current_packet = packet_queue_.front();

	if (VSOMEIP_MAX_TCP_MESSAGE_SIZE - current_packet.size() >=
		VSOMEIP_STATIC_HEADER_SIZE + VSOMEIP_MAGIC_COOKIE_SIZE) {
		current_packet.insert(
			current_packet.begin(),
			data,
			data + sizeof(data)
		);
	} else {
		// TODO: log "Packet full: cannot insert magic cookie"
	}
}

} // namespace vsomeip
