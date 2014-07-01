// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include <vsomeip/defines.hpp>

#include "../include/server_endpoint_impl.hpp"
#include "../../configuration/include/internal.hpp"

namespace vsomeip {

template < typename Protocol, int MaxBufferSize >
server_endpoint_impl< Protocol, MaxBufferSize >::server_endpoint_impl(
		std::shared_ptr< endpoint_host > _host, endpoint_type _local, boost::asio::io_service &_io)
	: endpoint_impl< MaxBufferSize >(_host, _io),
	  current_queue_(packet_queues_.end()),
	  flush_timer_(_io) {
}

template < typename Protocol, int MaxBufferSize >
bool server_endpoint_impl< Protocol, MaxBufferSize >::is_client() const {
	return false;
}

template < typename Protocol, int MaxBufferSize >
bool server_endpoint_impl< Protocol, MaxBufferSize >::send(
		const uint8_t *_data, uint32_t _size, bool _flush) {

	bool is_sent(false);
	if (VSOMEIP_CLIENT_POS_MAX < _size) {
		std::unique_lock< std::mutex > its_lock(mutex_);

		client_t its_client;
		std::memcpy(&its_client, &_data[VSOMEIP_CLIENT_POS_MIN], sizeof(client_t));

		auto found_target = clients_.find(its_client);
		if (found_target != clients_.end()) {
			bool is_queue_empty(packet_queues_.empty());

			endpoint_type its_target = found_target->second;

			// find queue and packetizer (buffer)
			std::deque< std::vector< uint8_t > >& target_packet_queue
				= packet_queues_[its_target];
			std::vector< uint8_t >& target_packetizer
				= packetizer_[its_target];

			// if the current_queue is not yet set, set it to newly created one
			if (current_queue_ == packet_queues_.end())
				current_queue_ = packet_queues_.find(its_target);

			if (target_packetizer.size() + _size > MaxBufferSize) {
				target_packet_queue.push_back(target_packetizer);
				target_packetizer.clear();

				if (is_queue_empty)
					send_queued();
			}

			target_packetizer.insert(target_packetizer.end(), _data, _data + _size);

			if (_flush) {
				flush_timer_.cancel();

				target_packet_queue.push_back(target_packetizer);
				target_packetizer.clear();

				if (is_queue_empty)
					send_queued();
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

	return is_sent;
}

template < typename Protocol, int MaxBufferSize >
bool server_endpoint_impl< Protocol, MaxBufferSize >::flush() {
	bool is_flushed = false;
#if 0
	auto i = packetizer_.find(_target);
	if (i != packetizer_.end() && !i->second.empty()) {
		std::deque< std::vector< uint8_t > >& target_packet_queue
				= packet_queues_[i->first];
		target_packet_queue.push_back(i->second);
		i->second.clear();

		flushed = true;
	}

	if (is_successful)
		send_queued();
#endif
	return is_flushed;
}

template < typename Protocol, int MaxBufferSize >
bool server_endpoint_impl< Protocol, MaxBufferSize >::set_next_queue() {
	if (current_queue_->second.empty())
		current_queue_ = packet_queues_.erase(current_queue_);

	if (packet_queues_.empty())
		return false;

	if (current_queue_ == packet_queues_.end())
		current_queue_ = packet_queues_.begin();

	if (!current_queue_->second.empty())
		return true;

	auto saved_current_queue = current_queue_;
	do {
		current_queue_++;
		if (current_queue_ == packet_queues_.end())
			current_queue_ = packet_queues_.begin();
	} while (current_queue_->second.empty() &&
			 current_queue_ != saved_current_queue);

	return !current_queue_->second.empty();
}

template < typename Protocol, int MaxBufferSize >
void server_endpoint_impl< Protocol, MaxBufferSize >::connect_cbk(
		boost::system::error_code const &_error) {
}

template < typename Protocol, int MaxBufferSize >
void server_endpoint_impl< Protocol, MaxBufferSize >::send_cbk(
		boost::system::error_code const &_error, std::size_t _bytes) {

    if (!_error) {
    	current_queue_->second.pop_front();
    	bool is_message_available(set_next_queue());
    	if (is_message_available) {
    		send_queued();
    	}
    }
}

template < typename Protocol, int MaxBufferSize >
void server_endpoint_impl< Protocol, MaxBufferSize >::flush_cbk(
		endpoint_type _target, const boost::system::error_code &_error_code) {
	if (!_error_code) {
		//(void)flush(_target);
	}
}

// Instantiate template
template class server_endpoint_impl< boost::asio::local::stream_protocol, VSOMEIP_MAX_LOCAL_MESSAGE_SIZE >;
template class server_endpoint_impl< boost::asio::ip::tcp, VSOMEIP_MAX_TCP_MESSAGE_SIZE >;
template class server_endpoint_impl< boost::asio::ip::udp, VSOMEIP_MAX_UDP_MESSAGE_SIZE >;

} // namespace vsomeip

