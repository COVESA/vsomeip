// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <sstream>

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/multicast.hpp>

#include <vsomeip/logger.hpp>

#include "../include/endpoint_host.hpp"
#include "../include/udp_server_endpoint_impl.hpp"
#include "../../message/include/byteorder.hpp"
#include "../../utility/include/utility.hpp"

namespace ip = boost::asio::ip;

namespace vsomeip {

udp_server_endpoint_impl::udp_server_endpoint_impl(
		std::shared_ptr< endpoint_host > _host, endpoint_type _local, boost::asio::io_service &_io)
	: server_endpoint_impl< ip::udp, VSOMEIP_MAX_UDP_MESSAGE_SIZE >(_host, _local, _io),
	  socket_(_io, _local) {
	boost::asio::socket_base::broadcast option(true);
	socket_.set_option(option);
}

udp_server_endpoint_impl::~udp_server_endpoint_impl() {
}

void udp_server_endpoint_impl::start() {
	receive();
}

void udp_server_endpoint_impl::stop() {
}

void udp_server_endpoint_impl::receive() {
	packet_buffer_ptr_t its_buffer
		= std::make_shared< packet_buffer_t >();
	socket_.async_receive_from(
		boost::asio::buffer(*its_buffer),
		remote_,
		std::bind(
			&udp_server_endpoint_impl::receive_cbk,
			std::dynamic_pointer_cast<
				udp_server_endpoint_impl >(shared_from_this()),
			its_buffer,
	        std::placeholders::_1,
	        std::placeholders::_2
		)
	);
}

void udp_server_endpoint_impl::restart() {
	receive();
}

void udp_server_endpoint_impl::send_queued(endpoint_type _target, message_buffer_ptr_t _buffer) {
#if 0
		std::stringstream msg;
		msg << "usei::sq: ";
		for (std::size_t i = 0; i < _data->size(); ++i)
			msg << std::hex << std::setw(2) << std::setfill('0') << (int)(*_data)[i] << " ";
		VSOMEIP_DEBUG << msg.str();
#endif
 	socket_.async_send_to(
		boost::asio::buffer(*_buffer),
		_target,
		std::bind(
			&udp_server_endpoint_base_impl::send_cbk,
			shared_from_this(),
			_buffer,
			std::placeholders::_1,
			std::placeholders::_2
		)
	);
}

udp_server_endpoint_impl::endpoint_type udp_server_endpoint_impl::get_remote() const {
	return remote_;
}

void udp_server_endpoint_impl::join(const std::string &_multicast_address) {
	if (local_.address().is_v4()) {
		try {
			socket_.set_option(boost::asio::ip::udp::socket::reuse_address(true));
			socket_.set_option(boost::asio::ip::multicast::join_group(
							   boost::asio::ip::address::from_string(_multicast_address)));
		}
		catch (...) {

		}
	} else {
		// TODO: support multicast for IPv6
	}
}

void udp_server_endpoint_impl::leave(const std::string &_multicast_address) {
	if (local_.address().is_v4()) {
		try {
			socket_.set_option(boost::asio::ip::udp::socket::reuse_address(true));
			socket_.set_option(boost::asio::ip::multicast::leave_group(
							   boost::asio::ip::address::from_string(_multicast_address)));
		}
		catch (...) {

		}
	} else {
		// TODO: support multicast for IPv6
	}
}

bool udp_server_endpoint_impl::is_v4() const {
	return socket_.local_endpoint().address().is_v4();
}

bool udp_server_endpoint_impl::get_address(std::vector< byte_t > &_address) const {
	boost::asio::ip::address its_address = socket_.local_endpoint().address();
	if (its_address.is_v4()) {
		boost::asio::ip::address_v4 its_address_v4 = its_address.to_v4();
		boost::asio::ip::address_v4::bytes_type its_bytes = its_address_v4.to_bytes();
		_address.assign(its_bytes.data(), its_bytes.data() + sizeof(its_bytes));
	} else {
		boost::asio::ip::address_v6 its_address_v6 = its_address.to_v6();
		boost::asio::ip::address_v6::bytes_type its_bytes = its_address_v6.to_bytes();
		_address.assign(its_bytes.data(), its_bytes.data() + sizeof(its_bytes));
	}
	return true;
}

unsigned short udp_server_endpoint_impl::get_port() const {
	return socket_.local_endpoint().port();
}

bool udp_server_endpoint_impl::is_udp() const {
	return false;
}

// TODO: find a better way to structure the receive functions
void udp_server_endpoint_impl::receive_cbk(
		packet_buffer_ptr_t _buffer,
		boost::system::error_code const &_error, std::size_t _bytes) {
#if 0
	std::stringstream msg;
	msg << "usei::rcb(" << _error.message() << "): ";
	for (std::size_t i = 0; i < _bytes; ++i)
		msg << std::hex << std::setw(2) << std::setfill('0') << (int)(*_buffer)[i] << " ";
	VSOMEIP_DEBUG << msg.str();
#endif
	if (!_error && 0 < _bytes) {
		message_.insert(message_.end(), _buffer->begin(), _buffer->begin() + _bytes);

		bool has_full_message;
		do {
			uint32_t current_message_size = utility::get_message_size(message_);
			has_full_message = (current_message_size > 0 && current_message_size <= message_.size());
			if (has_full_message) {
				if (utility::is_request(message_[VSOMEIP_MESSAGE_TYPE_POS])) {
					client_t its_client;
					std::memcpy(&its_client, &message_[VSOMEIP_CLIENT_POS_MIN], sizeof(client_t));
					session_t its_session;
					std::memcpy(&its_session, &message_[VSOMEIP_SESSION_POS_MIN], sizeof(session_t));
					clients_[its_client][its_session] = remote_;
				}

				this->host_->on_message(&message_[0], current_message_size, this);
				message_.erase(message_.begin(), message_.begin() + current_message_size);
			}
		} while (has_full_message);

		restart();
	} else {
		receive();
	}
}

} // namespace vsomeip
