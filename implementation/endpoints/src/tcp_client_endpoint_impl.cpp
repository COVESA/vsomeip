// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <boost/asio/write.hpp>

#include <vsomeip/defines.hpp>
#include <vsomeip/logger.hpp>

#include "../include/tcp_client_endpoint_impl.hpp"

namespace ip = boost::asio::ip;

namespace vsomeip {

tcp_client_endpoint_impl::tcp_client_endpoint_impl(
		std::shared_ptr< endpoint_host > _host, endpoint_type _remote, boost::asio::io_service &_io)
	: tcp_client_endpoint_base_impl(_host, _remote, _io) {
	is_supporting_magic_cookies_ = true;
}

tcp_client_endpoint_impl::~tcp_client_endpoint_impl() {
}

void tcp_client_endpoint_impl::start() {
	connect();
}

void tcp_client_endpoint_impl::connect() {
	socket_.open(remote_.protocol());

	// Nagle algorithm off
	ip::tcp::no_delay option;
	socket_.set_option(option);

	socket_.async_connect(
		remote_,
		std::bind(
			&tcp_client_endpoint_base_impl::connect_cbk,
			shared_from_this(),
			std::placeholders::_1
		)
	);
}

void tcp_client_endpoint_impl::receive() {
	packet_buffer_ptr_t its_buffer
		= std::make_shared< packet_buffer_t >();
	socket_.async_receive(
		boost::asio::buffer(*its_buffer),
		std::bind(
			&tcp_client_endpoint_base_impl::receive_cbk,
			shared_from_this(),
			its_buffer,
			std::placeholders::_1,
			std::placeholders::_2
		)
	);
}

void tcp_client_endpoint_impl::send_queued(message_buffer_ptr_t _buffer) {
	if (has_enabled_magic_cookies_)
		send_magic_cookie(_buffer);
	boost::asio::async_write(
		socket_,
		boost::asio::buffer(*_buffer),
		std::bind(
			&tcp_client_endpoint_base_impl::send_cbk,
			shared_from_this(),
			_buffer,
			std::placeholders::_1,
			std::placeholders::_2
		)
	);
}

void tcp_client_endpoint_impl::send_magic_cookie(message_buffer_ptr_t &_buffer) {
	static uint8_t data[] = { 0xFF, 0xFF, 0x00, 0x00,
							  0x00, 0x00, 0x00, 0x08,
							  0xDE, 0xAD, 0xBE, 0xEF,
							  0x01, 0x01, 0x01, 0x00 };
	if (VSOMEIP_MAX_TCP_MESSAGE_SIZE - _buffer->size() >=
		VSOMEIP_SOMEIP_HEADER_SIZE + VSOMEIP_SOMEIP_MAGIC_COOKIE_SIZE) {
		_buffer->insert(
			_buffer->begin(),
			data,
			data + sizeof(data)
		);
	} else {
		VSOMEIP_WARNING << "Packet full. Cannot insert magic cookie!";
	}
}

void tcp_client_endpoint_impl::join(const std::string &) {
}

void tcp_client_endpoint_impl::leave(const std::string &) {
}

} // namespace vsomeip
