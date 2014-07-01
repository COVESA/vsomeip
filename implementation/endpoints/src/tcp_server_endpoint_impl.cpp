// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <deque>

#include <boost/asio/write.hpp>

#include "../include/endpoint_host.hpp"
#include "../include/tcp_server_endpoint_impl.hpp"
#include "../../utility/include/utility.hpp"

namespace ip = boost::asio::ip;

namespace vsomeip {

tcp_server_endpoint_impl::tcp_server_endpoint_impl(
		std::shared_ptr< endpoint_host > _host, endpoint_type _local, boost::asio::io_service &_io)
	: tcp_server_endpoint_base_impl(_host, _local, _io),
	  acceptor_(_io, _local) {
	is_supporting_magic_cookies_ = true;
}

tcp_server_endpoint_impl::~tcp_server_endpoint_impl() {
}

void tcp_server_endpoint_impl::start() {
	connection::ptr new_connection = connection::create(this);

	acceptor_.async_accept(
		new_connection->get_socket(),
		std::bind(
			&tcp_server_endpoint_impl::accept_cbk,
			std::dynamic_pointer_cast< tcp_server_endpoint_impl >(shared_from_this()),
			new_connection,
			std::placeholders::_1
		)
	);
}

void tcp_server_endpoint_impl::stop() {
	for (auto& i : connections_)
		i.second->stop();
	acceptor_.close();
}

void tcp_server_endpoint_impl::receive() {
	// intentionally left empty
}

void tcp_server_endpoint_impl::restart() {
	current_->start();
}

const uint8_t * tcp_server_endpoint_impl::get_buffer() const {
	return current_->get_buffer();
}

void tcp_server_endpoint_impl::send_queued() {
	auto connection_iterator = connections_.find(current_queue_->first);
	if (connection_iterator != connections_.end())
		connection_iterator->second->send_queued();
}

tcp_server_endpoint_impl::endpoint_type tcp_server_endpoint_impl::get_remote() const {
	return current_->get_socket().remote_endpoint();
}

void tcp_server_endpoint_impl::accept_cbk(
		connection::ptr _connection, boost::system::error_code const &_error) {

	if (!_error) {
			socket_type &new_connection_socket = _connection->get_socket();
			endpoint_type remote = new_connection_socket.remote_endpoint();

			connections_[remote] = _connection;
			_connection->start();
		}

		start();
}

void tcp_server_endpoint_impl::join(const std::string &) {
}

void tcp_server_endpoint_impl::leave(const std::string &) {
}


///////////////////////////////////////////////////////////////////////////////
// class tcp_service_impl::connection
///////////////////////////////////////////////////////////////////////////////
tcp_server_endpoint_impl::connection::connection(tcp_server_endpoint_impl *_server)
	: socket_(_server->service_), server_(_server) {
}

tcp_server_endpoint_impl::connection::ptr
tcp_server_endpoint_impl::connection::create(tcp_server_endpoint_impl *_server) {
	return ptr(new connection(_server));
}

tcp_server_endpoint_impl::socket_type & tcp_server_endpoint_impl::connection::get_socket() {
	return socket_;
}

const uint8_t * tcp_server_endpoint_impl::connection::get_buffer() const {
	return buffer_.data();
}

void tcp_server_endpoint_impl::connection::start() {
	socket_.async_receive(
		boost::asio::buffer(buffer_),
		std::bind(
			&tcp_server_endpoint_impl::connection::receive_cbk,
			shared_from_this(),
			std::placeholders::_1,
			std::placeholders::_2
		)
	);
}

void tcp_server_endpoint_impl::connection::stop() {
	socket_.close();
}

void tcp_server_endpoint_impl::connection::send_queued() {
	if (server_->has_enabled_magic_cookies_)
		send_magic_cookie();

	std::deque<std::vector<uint8_t>> &current_queue
		= server_->current_queue_->second;

	boost::asio::async_write(
		socket_,
		boost::asio::buffer(
			&current_queue.front()[0],
			current_queue.front().size()
		),
		std::bind(
			&tcp_server_endpoint_base_impl::send_cbk,
			server_->shared_from_this(),
			std::placeholders::_1,
			std::placeholders::_2
		)
	);
}

void tcp_server_endpoint_impl::connection::send_magic_cookie() {
	static uint8_t data[] = { 0xFF, 0xFF, 0x80, 0x00,
							   0x00, 0x00, 0x00, 0x08,
							   0xDE, 0xAD, 0xBE, 0xEF,
							   0x01, 0x01, 0x02, 0x00 };

	std::vector<uint8_t>& current_packet
		= server_->current_queue_->second.front();

	if (VSOMEIP_MAX_TCP_MESSAGE_SIZE - current_packet.size() >=
		VSOMEIP_SOMEIP_HEADER_SIZE + VSOMEIP_SOMEIP_MAGIC_COOKIE_SIZE) {
		current_packet.insert(
			current_packet.begin(),
			data,
			data + sizeof(data)
		);
	}
}

void tcp_server_endpoint_impl::connection::receive_cbk(
		boost::system::error_code const &_error, std::size_t _bytes) {
	if (!_error && 0 < _bytes) {
		const uint8_t *buffer = get_buffer();
		message_.insert(message_.end(), buffer, buffer + _bytes);

		bool has_full_message;
		do {
			uint32_t current_message_size = utility::get_message_size(message_);
			has_full_message = (current_message_size > 0 && current_message_size <= message_.size());
			if (has_full_message) {
				if (utility::is_request(message_[VSOMEIP_MESSAGE_TYPE_POS])) {
					client_t its_client;
					std::memcpy(&its_client, &message_[VSOMEIP_CLIENT_POS_MIN], sizeof(client_t));
					server_->clients_[its_client] = socket_.remote_endpoint();
				}

				server_->host_->on_message(&message_[0], current_message_size, server_);
				message_.erase(message_.begin(), message_.begin() + current_message_size);
			}
		} while (has_full_message);

		server_->restart();
	} else {
		server_->receive();
	}
}

} // namespace vsomeip
