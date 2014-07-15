// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>

#include <boost/asio/write.hpp>

#include <vsomeip/constants.hpp>
#include <vsomeip/defines.hpp>
#include <vsomeip/logger.hpp>

#include "../include/endpoint_host.hpp"
#include "../include/tcp_client_endpoint_impl.hpp"
#include "../../utility/include/utility.hpp"

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
			&tcp_client_endpoint_impl::receive_cbk,
			std::dynamic_pointer_cast< tcp_client_endpoint_impl >(shared_from_this()),
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

bool tcp_client_endpoint_impl::is_magic_cookie() const {
	return (0 == std::memcmp(server_cookie, &message_[0], sizeof(server_cookie)));
}

void tcp_client_endpoint_impl::send_magic_cookie(message_buffer_ptr_t &_buffer) {
	if (VSOMEIP_MAX_TCP_MESSAGE_SIZE - _buffer->size() >=
		VSOMEIP_SOMEIP_HEADER_SIZE + VSOMEIP_SOMEIP_MAGIC_COOKIE_SIZE) {
		_buffer->insert(
			_buffer->begin(),
			client_cookie,
			client_cookie + sizeof(client_cookie)
		);
	} else {
		VSOMEIP_WARNING << "Packet full. Cannot insert magic cookie!";
	}
}

void tcp_client_endpoint_impl::join(const std::string &) {
}

void tcp_client_endpoint_impl::leave(const std::string &) {
}

void tcp_client_endpoint_impl::receive_cbk(
		packet_buffer_ptr_t _buffer,
		boost::system::error_code const &_error, std::size_t _bytes) {
#if 0
	std::stringstream msg;
	msg << "cei::rcb (" << _error.message() << "): ";
	for (std::size_t i = 0; i < _bytes; ++i)
		msg << std::hex << std::setw(2) << std::setfill('0') << (int)(*_buffer)[i] << " ";
	VSOMEIP_DEBUG << msg.str();
#endif
	if (!_error && 0 < _bytes) {
		this->message_.insert(this->message_.end(), _buffer->begin(), _buffer->begin() + _bytes);

		bool has_full_message;
		do {
			uint32_t current_message_size = utility::get_message_size(this->message_);
			has_full_message = (current_message_size > 0 && current_message_size <= this->message_.size());
			if (has_full_message) {
				if (is_magic_cookie()) {
					has_enabled_magic_cookies_ = true;
				} else {
					host_->on_message(&this->message_[0], current_message_size, this);
				}
				this->message_.erase(this->message_.begin(), this->message_.begin() + current_message_size);
			} else if (has_enabled_magic_cookies_ && this->message_.size() > 0){
				// Note that the following will be done each time a message
				// is not (yet) completely available. If no magic cookie can
				// be found, the message data is not touched.
				has_full_message = resync_on_magic_cookie(message_);
			} else if (message_.size() > VSOMEIP_MAX_TCP_MESSAGE_SIZE) {
				VSOMEIP_ERROR << "Message exceeds maximum message size. Resetting receiver.";
				this->message_.clear();
			}
		} while (has_full_message);
		restart();
	} else {
		receive();
	}
}

} // namespace vsomeip
