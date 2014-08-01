// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <deque>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <boost/asio/write.hpp>

#include <vsomeip/logger.hpp>

#include "../include/endpoint_host.hpp"
#include "../include/local_server_endpoint_impl.hpp"

namespace vsomeip {

local_server_endpoint_impl::local_server_endpoint_impl(
		std::shared_ptr< endpoint_host > _host, endpoint_type _local, boost::asio::io_service &_io)
	: local_server_endpoint_base_impl(_host, _local, _io),
	  acceptor_(_io, _local) {
	is_supporting_magic_cookies_ = false;
}

local_server_endpoint_impl::~local_server_endpoint_impl() {
}

void local_server_endpoint_impl::start() {
	connection::ptr new_connection = connection::create(this);

	acceptor_.async_accept(
		new_connection->get_socket(),
		std::bind(
			&local_server_endpoint_impl::accept_cbk,
			std::dynamic_pointer_cast< local_server_endpoint_impl >(shared_from_this()),
			new_connection,
			std::placeholders::_1
		)
	);
}

void local_server_endpoint_impl::stop() {

}

bool local_server_endpoint_impl::send_to(const boost::asio::ip::address &_address, uint16_t _port,
                                         const byte_t *_data, uint32_t _size, bool _flush) {
  return false;
}

void local_server_endpoint_impl::send_queued(endpoint_type _target, message_buffer_ptr_t _buffer) {
	auto connection_iterator = connections_.find(_target);
	if (connection_iterator != connections_.end())
		connection_iterator->second->send_queued(_buffer);
}

void local_server_endpoint_impl::receive() {
	// intentionally left empty
}

void local_server_endpoint_impl::restart() {
	current_->start();
}

local_server_endpoint_impl::endpoint_type local_server_endpoint_impl::get_remote() const {
	return current_->get_socket().remote_endpoint();
}

bool local_server_endpoint_impl::get_multicast(
		service_t, event_t, local_server_endpoint_impl::endpoint_type &) const {
	return false;
}

void local_server_endpoint_impl::remove_connection(local_server_endpoint_impl::connection *_connection) {
	std::map< endpoint_type, connection::ptr >::iterator i = connections_.end();
	for (i = connections_.begin(); i != connections_.end(); i++) {
		if (i->second.get() == _connection)
			break;
	}

	if (i != connections_.end()) {
		connections_.erase(i);
	}
}

void local_server_endpoint_impl::accept_cbk(
		connection::ptr _connection, boost::system::error_code const &_error) {

	if (!_error) {
		socket_type &new_connection_socket = _connection->get_socket();
		endpoint_type remote = new_connection_socket.remote_endpoint();

		connections_[remote] = _connection;
		_connection->start();
	}

	start();
}

///////////////////////////////////////////////////////////////////////////////
// class tcp_service_impl::connection
///////////////////////////////////////////////////////////////////////////////
local_server_endpoint_impl::connection::connection(local_server_endpoint_impl *_server)
	: socket_(_server->service_), server_(_server) {
}

local_server_endpoint_impl::connection::ptr
local_server_endpoint_impl::connection::create(local_server_endpoint_impl *_server) {
	return ptr(new connection(_server));
}

local_server_endpoint_impl::socket_type & local_server_endpoint_impl::connection::get_socket() {
	return socket_;
}

void local_server_endpoint_impl::connection::start() {
	packet_buffer_ptr_t its_buffer
		= std::make_shared< packet_buffer_t >();
	socket_.async_receive(
		boost::asio::buffer(*its_buffer),
		std::bind(
			&local_server_endpoint_impl::connection::receive_cbk,
			shared_from_this(),
			its_buffer,
			std::placeholders::_1,
			std::placeholders::_2
		)
	);
}

void local_server_endpoint_impl::connection::send_queued(message_buffer_ptr_t _buffer) {
#if 0
		std::stringstream msg;
		msg << "lse::sq: ";
		for (std::size_t i = 0; i < _buffer->size(); i++)
			msg << std::setw(2) << std::setfill('0') << std::hex << (int)(*_buffer)[i] << " ";
		VSOMEIP_DEBUG << msg.str();
#endif
	boost::asio::async_write(
		socket_,
		boost::asio::buffer(*_buffer),
		std::bind(
			&local_server_endpoint_base_impl::send_cbk,
			server_->shared_from_this(),
			_buffer,
			std::placeholders::_1,
			std::placeholders::_2
		)
	);
}

void local_server_endpoint_impl::connection::send_magic_cookie() {
}

void local_server_endpoint_impl::connection::receive_cbk(
		packet_buffer_ptr_t _buffer,
		boost::system::error_code const &_error, std::size_t _bytes) {

	static std::size_t its_start = -1;
	std::size_t its_end;

	if (!_error && 0 < _bytes) {
#if 0
		std::stringstream msg;
		msg << "lse::c<" << this << ">rcb: ";
		for (std::size_t i = 0; i < _bytes; i++)
			msg << std::setw(2) << std::setfill('0') << std::hex << (int)(*_buffer)[i] << " ";
		VSOMEIP_DEBUG << msg.str();
#endif

		message_.insert(message_.end(), _buffer->begin(), _buffer->begin() + _bytes);

		do {
			if (its_start == -1) {
				its_start = 0;
				while (its_start + 3 < message_.size() &&
					(message_[its_start] != 0x67 || message_[its_start+1] != 0x37 ||
					 message_[its_start+2] != 0x6d || message_[its_start+3] != 0x07)) {
					its_start ++;
				}

				its_start = (its_start + 3 == message_.size() ? -1 : its_start+4);
			}

			if (its_start != -1) {
				its_end = its_start;
				while (its_end + 3 < message_.size() &&
					(message_[its_end] != 0x07 || message_[its_end+1] != 0x6d ||
					 message_[its_end+2] != 0x37 || message_[its_end+3] != 0x67)) {
					its_end ++;
				}
			}

			if (its_start != -1 && its_end+3 < message_.size()) {
				server_->host_->on_message(&message_[its_start], its_end - its_start, server_);
				message_.erase(message_.begin(), message_.begin() + its_end + 4);
				its_start = -1;
			}
		} while (message_.size() > 0 && its_start == -1);
	}

	if (_error == boost::asio::error::misc_errors::eof) {
		server_->remove_connection(this);
	} else {
		start();
	}
}

} // namespace vsomeip
