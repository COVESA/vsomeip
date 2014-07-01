// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <iomanip>

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include <vsomeip/defines.hpp>

#include "../include/client_endpoint_impl.hpp"
#include "../include/endpoint_host.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../utility/include/utility.hpp"

namespace vsomeip {

template < typename Protocol, int MaxBufferSize >
client_endpoint_impl< Protocol, MaxBufferSize >::client_endpoint_impl(
		std::shared_ptr< endpoint_host > _host,
		endpoint_type _remote,
		boost::asio::io_service &_io)
	: endpoint_impl< MaxBufferSize >(_host, _io),
	  socket_(_io),
	  connect_timer_(_io),
	  flush_timer_(_io),
	  remote_(_remote),
	  connect_timeout_(VSOMEIP_DEFAULT_CONNECT_TIMEOUT), // TODO: use config variable
	  is_connected_(false) {
}

template < typename Protocol, int MaxBufferSize >
client_endpoint_impl< Protocol, MaxBufferSize >::~client_endpoint_impl() {
}

template < typename Protocol, int MaxBufferSize >
const uint8_t * client_endpoint_impl< Protocol, MaxBufferSize >::get_buffer() const {
	return buffer_.data();
}

template < typename Protocol, int MaxBufferSize >
bool client_endpoint_impl< Protocol, MaxBufferSize >::is_client() const {
	return true;
}

template < typename Protocol, int MaxBufferSize >
void client_endpoint_impl< Protocol, MaxBufferSize >::stop() {
	if (socket_.is_open()) {
		socket_.close();
	}
}

template < typename Protocol, int MaxBufferSize >
void client_endpoint_impl< Protocol, MaxBufferSize >::restart() {
	receive();
}

template < typename Protocol, int MaxBufferSize >
bool client_endpoint_impl< Protocol, MaxBufferSize >::send(
		const uint8_t *_data, uint32_t _size, bool _flush) {
	std::unique_lock< std::mutex > its_lock(mutex_);

	bool is_queue_empty(packet_queue_.empty());

	if (packetizer_.size() + _size > MaxBufferSize) {
		packet_queue_.push_back(packetizer_);
		packetizer_.clear();
		if (is_queue_empty && is_connected_)
			send_queued();
	}

	packetizer_.insert(packetizer_.end(), _data, _data + _size);

	if (_flush) {
		flush_timer_.cancel();
		packet_queue_.push_back(packetizer_);
		packetizer_.clear();
		if (is_queue_empty && is_connected_)
			send_queued();
	} else {
		flush_timer_.expires_from_now(
			std::chrono::milliseconds(VSOMEIP_DEFAULT_FLUSH_TIMEOUT)); // TODO: use config variable
		flush_timer_.async_wait(
			std::bind(
				&client_endpoint_impl<Protocol, MaxBufferSize>::flush_cbk,
				this->shared_from_this(),
				std::placeholders::_1
			)
		);
	}

	return true;
}

template < typename Protocol, int MaxBufferSize >
bool client_endpoint_impl< Protocol, MaxBufferSize >::flush() {
	bool is_successful(true);

	if (!packetizer_.empty()) {
		packet_queue_.push_back(packetizer_);
		packetizer_.clear();
		send_queued();
	} else {
		is_successful = false;
	}

	return is_successful;
}

template < typename Protocol, int MaxBufferSize >
void client_endpoint_impl< Protocol, MaxBufferSize >::connect_cbk(
		boost::system::error_code const &_error) {

	if (_error) {
		socket_.close();

		connect_timer_.expires_from_now(
			std::chrono::milliseconds(connect_timeout_));
		connect_timer_.async_wait(
			std::bind(
				&client_endpoint_impl<Protocol, MaxBufferSize>::wait_connect_cbk,
				this->shared_from_this(),
				std::placeholders::_1
			)
		);

		// next time we wait longer
		connect_timeout_ <<= 1;
	} else {
		connect_timer_.cancel();
		connect_timeout_ = VSOMEIP_DEFAULT_CONNECT_TIMEOUT; // TODO: use config variable
		is_connected_ = true;
		if (!packet_queue_.empty()) {
			send_queued();
		}
		receive();
	}
}

template < typename Protocol, int MaxBufferSize >
void client_endpoint_impl< Protocol, MaxBufferSize >::wait_connect_cbk(
		boost::system::error_code const &_error) {

	if (!_error) {
		connect();
	}
}

template < typename Protocol, int MaxBufferSize >
void client_endpoint_impl< Protocol, MaxBufferSize >::send_cbk(
		boost::system::error_code const &_error, std::size_t _bytes) {

	if (!_error && _bytes > 0) {
		packet_queue_.pop_front();

		if (!packet_queue_.empty()) {
			send_queued();
		}
	} else {
		if (_error == boost::asio::error::broken_pipe) {
			is_connected_ = false;
			socket_.close();
			connect();
		}
	}
}

template < typename Protocol, int MaxBufferSize >
void client_endpoint_impl< Protocol, MaxBufferSize >::flush_cbk(
		boost::system::error_code const &_error) {
	if (!_error) {
		(void)flush();
	}
}

template < typename Protocol, int MaxBufferSize >
void client_endpoint_impl< Protocol, MaxBufferSize >::receive_cbk(
		boost::system::error_code const &_error, std::size_t _bytes) {

	static uint32_t message_counter = 0;

	if (!_error && 0 < _bytes) {
		const uint8_t *buffer = get_buffer();
		message_.insert(message_.end(), buffer, buffer + _bytes);

		bool has_full_message;
		do {
			uint32_t current_message_size = utility::get_message_size(message_);
			has_full_message = (current_message_size > 0 && current_message_size <= message_.size());
			if (has_full_message) {
				this->host_->on_message(&message_[0], current_message_size, this);
				message_.erase(message_.begin(), message_.begin() + current_message_size);
			}
		} while (has_full_message);

		restart();
	} else {
		receive();
	}
}

// Instantiate template
template class client_endpoint_impl< boost::asio::local::stream_protocol, VSOMEIP_MAX_LOCAL_MESSAGE_SIZE >;
template class client_endpoint_impl< boost::asio::ip::tcp, VSOMEIP_MAX_TCP_MESSAGE_SIZE >;
template class client_endpoint_impl< boost::asio::ip::udp, VSOMEIP_MAX_UDP_MESSAGE_SIZE >;

} // namespace vsomeip

