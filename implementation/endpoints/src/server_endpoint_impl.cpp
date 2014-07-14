// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <sstream>

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include <vsomeip/defines.hpp>
#include <vsomeip/logger.hpp>

#include "../include/server_endpoint_impl.hpp"
#include "../../configuration/include/internal.hpp"

namespace vsomeip {

template < typename Protocol, int MaxBufferSize >
server_endpoint_impl< Protocol, MaxBufferSize >::server_endpoint_impl(
		std::shared_ptr< endpoint_host > _host, endpoint_type _local, boost::asio::io_service &_io)
	: endpoint_impl< MaxBufferSize >(_host, _io),
	  flush_timer_(_io) {
}

template < typename Protocol, int MaxBufferSize >
bool server_endpoint_impl< Protocol, MaxBufferSize >::is_client() const {
	return false;
}

template < typename Protocol, int MaxBufferSize >
bool server_endpoint_impl< Protocol, MaxBufferSize >::send(
		const uint8_t *_data, uint32_t _size, bool _flush) {
#if 0
	std::stringstream msg;
	msg << "sei::send ";
	for (uint32_t i = 0; i < _size; i++)
		msg << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
	VSOMEIP_DEBUG << msg.str();
#endif
	bool is_sent(false);
	if (VSOMEIP_SESSION_POS_MAX < _size) {
		std::unique_lock< std::mutex > its_lock(mutex_);

		client_t its_client;
		std::memcpy(&its_client, &_data[VSOMEIP_CLIENT_POS_MIN], sizeof(client_t));
		session_t its_session;
		std::memcpy(&its_session, &_data[VSOMEIP_SESSION_POS_MIN], sizeof(session_t));

		auto found_client = clients_.find(its_client);
		if (found_client != clients_.end()) {
			auto found_session = found_client->second.find(its_session);
			if (found_session != found_client->second.end()) {
				endpoint_type its_target = found_session->second;

				// find queue and packetizer (buffer)
				std::shared_ptr< buffer_t > target_packetizer;

				auto found_packetizer = packetizer_.find(its_target);
				if (found_packetizer != packetizer_.end()) {
					target_packetizer = found_packetizer->second;
				} else {
					target_packetizer = std::make_shared< buffer_t >();
					packetizer_.insert(std::make_pair(its_target, target_packetizer));
				}

				if (target_packetizer->size() + _size > MaxBufferSize) {
					send_queued(its_target, target_packetizer);
					packetizer_[its_target] = std::make_shared< buffer_t >();
				}

				target_packetizer->insert(target_packetizer->end(), _data, _data + _size);

				if (_flush) {
					flush_timer_.cancel();
					send_queued(its_target, target_packetizer);
					packetizer_[its_target] = std::make_shared< buffer_t >();
				} else {
					std::chrono::milliseconds flush_timeout(VSOMEIP_DEFAULT_FLUSH_TIMEOUT);
					flush_timer_.expires_from_now(flush_timeout); // TODO: use configured value
					flush_timer_.async_wait(
						std::bind(
							&server_endpoint_impl<Protocol, MaxBufferSize>::flush_cbk,
							this->shared_from_this(),
							its_target,
							std::placeholders::_1
						)
					);
				}
				is_sent = true;
			}
		}
	}

	return is_sent;
}

template < typename Protocol, int MaxBufferSize >
bool server_endpoint_impl< Protocol, MaxBufferSize >::flush(endpoint_type _target) {
	bool is_flushed = false;
	std::unique_lock< std::mutex > its_lock(mutex_);
	auto i = packetizer_.find(_target);
	if (i != packetizer_.end() && !i->second->empty()) {
		send_queued(_target, i->second);
		i->second = std::make_shared< buffer_t >();
		is_flushed = true;
	}

	return is_flushed;
}

template < typename Protocol, int MaxBufferSize >
void server_endpoint_impl< Protocol, MaxBufferSize >::connect_cbk(
		boost::system::error_code const &_error) {
}

template < typename Protocol, int MaxBufferSize >
void server_endpoint_impl< Protocol, MaxBufferSize >::send_cbk(
		buffer_ptr_t _buffer,
		boost::system::error_code const &_error, std::size_t _bytes) {
#if 0
		std::stringstream msg;
		msg << "sei::scb (" << _error.message() << "): ";
		for (std::size_t i = 0; i < _buffer->size(); ++i)
			msg << std::hex << std::setw(2) << std::setfill('0') << (int)(*_buffer)[i] << " ";
		VSOMEIP_DEBUG << msg.str();
#endif
}

template < typename Protocol, int MaxBufferSize >
void server_endpoint_impl< Protocol, MaxBufferSize >::flush_cbk(
		endpoint_type _target, const boost::system::error_code &_error_code) {
	if (!_error_code) {
		(void)flush(_target);
	}
}

// Instantiate template
template class server_endpoint_impl< boost::asio::local::stream_protocol, VSOMEIP_MAX_LOCAL_MESSAGE_SIZE >;
template class server_endpoint_impl< boost::asio::ip::tcp, VSOMEIP_MAX_TCP_MESSAGE_SIZE >;
template class server_endpoint_impl< boost::asio::ip::udp, VSOMEIP_MAX_UDP_MESSAGE_SIZE >;

} // namespace vsomeip
