//
// client_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <chrono>

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/bind.hpp>

#include <vsomeip/config.hpp>
#include <vsomeip/endpoint.hpp>
#include <vsomeip_internal/client_impl.hpp>
#include <vsomeip_internal/log_macros.hpp>
#include <vsomeip_internal/managing_application.hpp>

namespace vsomeip {

template < typename Protocol, int MaxBufferSize >
client_impl< Protocol, MaxBufferSize >::client_impl(
		managing_application *_owner, const endpoint *_location)
	: participant_impl< MaxBufferSize >(_owner, _location),
	  socket_(_owner->get_io_service()),
	  connect_timer_(_owner->get_io_service()),
	  flush_timer_(_owner->get_io_service()),
	  local_(boost::asio::ip::address::from_string(_location->get_address()),
			 _location->get_port()),
	  connect_timeout_(VSOMEIP_CONNECT_TIMEOUT),
	  is_connected_(false) {
}

template < typename Protocol, int MaxBufferSize >
client_impl< Protocol, MaxBufferSize >::~client_impl() {
}

template < typename Protocol, int MaxBufferSize >
const uint8_t * client_impl< Protocol, MaxBufferSize >::get_buffer() const {
	return buffer_.data();
}

template < typename Protocol, int MaxBufferSize >
bool client_impl< Protocol, MaxBufferSize >::is_client() const {
	return true;
}

template < typename Protocol, int MaxBufferSize >
void client_impl< Protocol, MaxBufferSize >::stop() {
	if (socket_.is_open())
		socket_.close();
}

template < typename Protocol, int MaxBufferSize >
void client_impl< Protocol, MaxBufferSize >::restart() {
	receive();
}

template < typename Protocol, int MaxBufferSize >
bool client_impl< Protocol, MaxBufferSize >::send(
		const uint8_t *_data, uint32_t _size, bool _flush) {

	bool is_queue_empty(packet_queue_.empty());

	if (packetizer_.size() + _size > MaxBufferSize) {
		//VSOMEIP_INFO << "Implicit flush because new message cannot be buffered.";
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
			std::chrono::milliseconds(VSOMEIP_FLUSH_TIMEOUT));
		flush_timer_.async_wait(
			boost::bind(
				&client_impl<Protocol, MaxBufferSize>::flush_cbk,
				this,
				boost::asio::placeholders::error
			)
		);
	}

	return true;
}

template < typename Protocol, int MaxBufferSize >
bool client_impl< Protocol, MaxBufferSize >::flush() {
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
void client_impl< Protocol, MaxBufferSize >::connect_cbk(
		boost::system::error_code const &_error) {

	if (_error) {
		socket_.close();

		connect_timer_.expires_from_now(
			std::chrono::milliseconds(connect_timeout_));
		connect_timer_.async_wait(
			boost::bind(
				&client_impl<Protocol, MaxBufferSize>::wait_connect_cbk,
				this,
				boost::asio::placeholders::error
			)
		);

		// next time we wait longer
		connect_timeout_ <<= 1;
	} else {
		connect_timer_.cancel();
		connect_timeout_ = VSOMEIP_CONNECT_TIMEOUT;
		is_connected_ = true;
		if (!packet_queue_.empty()) {
			send_queued();
		}
		receive();
	}
}

template < typename Protocol, int MaxBufferSize >
void client_impl< Protocol, MaxBufferSize >::wait_connect_cbk(
		boost::system::error_code const &_error) {

	if (!_error) {
		connect();
	}
}

template < typename Protocol, int MaxBufferSize >
void client_impl< Protocol, MaxBufferSize >::send_cbk(
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
void client_impl< Protocol, MaxBufferSize >::flush_cbk(
		boost::system::error_code const &_error) {
	if (!_error) {
		(void)flush();
	}
}

// Instatiate template
template class client_impl<boost::asio::ip::tcp, VSOMEIP_MAX_TCP_MESSAGE_SIZE>;
template class client_impl<boost::asio::ip::udp, VSOMEIP_MAX_UDP_MESSAGE_SIZE>;

} // namespace vsomeip

