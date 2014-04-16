//
// tcp_service_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright 2013, 2024 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <deque>

#include <boost/asio/placeholders.hpp>
#include <boost/asio/write.hpp>
#include <boost/bind.hpp>

#include <vsomeip/endpoint.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/factory.hpp>
#include <vsomeip_internal/log_macros.hpp>
#include <vsomeip_internal/managing_application_impl.hpp>
#include <vsomeip_internal/tcp_service_impl.hpp>

namespace ip = boost::asio::ip;

namespace vsomeip {

tcp_service_impl::tcp_service_impl(
		managing_application_impl *_owner, const endpoint *_location)
	: service_impl<ip::tcp, VSOMEIP_MAX_TCP_MESSAGE_SIZE>(_owner, _location),
	  acceptor_(_owner->get_service(),
			    ip::tcp::endpoint(
			    	(_location->get_version() == ip_protocol_version::V6 ?
			    			ip::tcp::v6() : ip::tcp::v4()),
			    _location->get_port())) {
	is_supporting_magic_cookies_ = true;
}

tcp_service_impl::~tcp_service_impl() {
}

void tcp_service_impl::start() {
	connection::ptr new_connection = connection::create(this);

	acceptor_.async_accept(
		new_connection->get_socket(),
		boost::bind(
			&tcp_service_impl::accept_cbk,
			this,
			new_connection,
			boost::asio::placeholders::error
		)
	);
}

void tcp_service_impl::stop() {

}

void tcp_service_impl::send_queued() {
	auto connection_iterator = connections_.find(current_queue_->first);
	if (connection_iterator != connections_.end())
		connection_iterator->second->send_queued();
	else
		VSOMEIP_ERROR << "No connection could not be found!";
}

void tcp_service_impl::receive() {
	// intentionally left empty
}

void tcp_service_impl::restart() {
	if (current_)
		current_->start();
}

ip_address tcp_service_impl::get_remote_address() const {
	return (current_ == 0 ?
				0 : current_->get_socket().
						remote_endpoint().address().to_string());
}

ip_port tcp_service_impl::get_remote_port() const {
	return (current_ == 0 ?
				0 : current_->get_socket().remote_endpoint().port());
}

ip_protocol tcp_service_impl::get_protocol() const {
	return ip_protocol::TCP;
}

const uint8_t * tcp_service_impl::get_buffer() const {
	return (current_ ? current_->get_buffer() : 0);
}

void tcp_service_impl::accept_cbk(
		connection::ptr _connection, boost::system::error_code const &_error) {

	if (!_error) {
			socket_type &new_connection_socket = _connection->get_socket();
			endpoint_type remote_endpoint = new_connection_socket.remote_endpoint();
			ip::address remote_address = remote_endpoint.address();
			endpoint *remote = factory::get_instance()->get_endpoint(
									remote_address.to_string(),
									remote_endpoint.port(),
									ip_protocol::TCP
							   );

			connections_[remote] = _connection;
			_connection->start();
		}

		start();
}

///////////////////////////////////////////////////////////////////////////////
// class tcp_service_impl::connection
///////////////////////////////////////////////////////////////////////////////
tcp_service_impl::connection::connection(tcp_service_impl *_owner)
	: socket_(_owner->service_), owner_(_owner) {
}

tcp_service_impl::connection::ptr
tcp_service_impl::connection::create(tcp_service_impl *_owner) {
	return ptr(new connection(_owner));
}

tcp_service_impl::socket_type & tcp_service_impl::connection::get_socket() {
	return socket_;
}

const uint8_t * tcp_service_impl::connection::get_buffer() const {
	return buffer_.data();
}

void tcp_service_impl::connection::start() {
	socket_.async_receive(
		boost::asio::buffer(buffer_),
		boost::bind(
			&tcp_service_impl::connection::receive_cbk,
			shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred
		)
	);
}

void tcp_service_impl::connection::send_queued() {
	if (owner_->has_enabled_magic_cookies_)
		send_magic_cookie();

	std::deque<std::vector<uint8_t>> &current_queue
		= owner_->current_queue_->second;

	boost::asio::async_write(
		socket_,
		boost::asio::buffer(
			&current_queue.front()[0],
			current_queue.front().size()
		),
		boost::bind(
			&tcp_service_base_impl::send_cbk,
			owner_,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred
		)
	);
}

void tcp_service_impl::connection::send_magic_cookie() {
	static uint8_t data[] = { 0xFF, 0xFF, 0x80, 0x00,
							   0x00, 0x00, 0x00, 0x08,
							   0xDE, 0xAD, 0xBE, 0xEF,
							   0x01, 0x01, 0x02, 0x00 };

	std::vector<uint8_t>& current_packet
		= owner_->current_queue_->second.front();

	if (VSOMEIP_MAX_TCP_MESSAGE_SIZE - current_packet.size() >=
		VSOMEIP_STATIC_HEADER_SIZE + VSOMEIP_MAGIC_COOKIE_SIZE) {
		current_packet.insert(
			current_packet.begin(),
			data,
			data + sizeof(data)
		);
	}
}

void tcp_service_impl::connection::receive_cbk(
		boost::system::error_code const &_error, std::size_t _bytes) {

	owner_->current_ = this;
	owner_->receive_cbk(_error, _bytes);
}

} // namespace vsomeip



