//
// tcp_service_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <boost/bind.hpp>
#include <boost/asio/buffer.hpp>

#include <boost/asio/placeholders.hpp>
#include <boost/asio/write.hpp>

#include <vsomeip/deserializer.hpp>
#include <vsomeip/endpoint.hpp>
#include <vsomeip/factory.hpp>
#include <vsomeip/serializer.hpp>
#include <vsomeip/internal/tcp_provider_impl.hpp>

namespace ip = boost::asio::ip;

namespace vsomeip {

tcp_provider_impl::tcp_provider_impl(
		factory *_factory,
		const endpoint *_endpoint,
		boost::asio::io_service &_is)
	: provider_base_impl(_factory, VSOMEIP_MAX_TCP_MESSAGE_SIZE, _is),
	  acceptor_(is_,
			    ip::tcp::endpoint((_endpoint->get_version() == ip_version::V6 ?
			    				  	   ip::tcp::v6() : ip::tcp::v4()),
			    				  _endpoint->get_port())),
	  version_(_endpoint->get_version()) {

	has_magic_cookies_ = true;
}

tcp_provider_impl::~tcp_provider_impl() {
}

void tcp_provider_impl::start() {
	connection::pointer new_connection = connection::create(this);

	acceptor_.async_accept(new_connection->get_socket(),
			boost::bind(&tcp_provider_impl::accepted, this,
						new_connection, boost::asio::placeholders::error));
}

void tcp_provider_impl::connect() {
}

void tcp_provider_impl::receive() {
}

void tcp_provider_impl::restart() {
	if (current_receiving_) {
		current_receiving_->start();
	}
}

void tcp_provider_impl::stop() {
}

void tcp_provider_impl::send_queued() {
	auto connection_iterator = connections_.find(current_queue_->first);
	if (connection_iterator != connections_.end())
		connection_iterator->second->send_queued();

	// TODO: log message in case the connection could not be found
}

ip_address tcp_provider_impl::get_remote_address() const {
	return (current_receiving_ == 0 ?
				0 : current_receiving_->get_socket().
						remote_endpoint().address().to_string());
}

ip_port tcp_provider_impl::get_remote_port() const {
	return (current_receiving_ == 0 ?
				0 : current_receiving_->get_socket().remote_endpoint().port());
}

ip_protocol tcp_provider_impl::get_protocol() const {
	return ip_protocol::TCP;
}

ip_version tcp_provider_impl::get_version() const {
	return version_;
}

const uint8_t * tcp_provider_impl::get_received() const {
	return (current_receiving_ == 0 ?
				0 : current_receiving_->get_received_data());
}

bool tcp_provider_impl::is_magic_cookie(
		message_id _message_id, length _length, request_id _request_id,
		protocol_version _protocol_version, interface_version _interface_version,
		message_type _message_type, return_code _return_code) const {
	return (_message_id	 == VSOMEIP_MAGIC_COOKIE_CLIENT_MESSAGE_ID &&
			 _length == VSOMEIP_MAGIC_COOKIE_LENGTH &&
			 _request_id == VSOMEIP_MAGIC_COOKIE_REQUEST_ID &&
			 _protocol_version == VSOMEIP_MAGIC_COOKIE_PROTOCOL_VERSION &&
			 _interface_version == VSOMEIP_MAGIC_COOKIE_INTERFACE_VERSION &&
			 _message_type == VSOMEIP_MAGIC_COOKIE_CLIENT_MESSAGE_TYPE &&
			 _return_code == VSOMEIP_MAGIC_COOKIE_RETURN_CODE);
}

void tcp_provider_impl::accepted(
		connection::pointer _connection,
		const boost::system::error_code& _error_code) {

	if (!_error_code) {
		ip::tcp::socket &new_connection_socket = _connection->get_socket();
		ip::tcp::endpoint remote_endpoint = new_connection_socket.remote_endpoint();
		ip::address remote_address = remote_endpoint.address();
		endpoint *remote = factory::get_default_factory()->get_endpoint(
								remote_address.to_string(),
								remote_endpoint.port(),
								ip_protocol::TCP,
								(remote_address.is_v4() ?
										ip_version::V4 : ip_version::V6));

		connections_[remote] = _connection;
		_connection->start();
	}

	start();
}

// Inner class connection
tcp_provider_impl::connection::connection(tcp_provider_impl *_owner)
	: socket_(_owner->is_), owner_(_owner) {
}

tcp_provider_impl::connection::pointer
tcp_provider_impl::connection::create(tcp_provider_impl *_owner) {
	return pointer(new connection(_owner));
}

boost::asio::ip::tcp::socket &
tcp_provider_impl::connection::get_socket() {
	return socket_;
}

void
tcp_provider_impl::connection::start() {
	socket_.async_receive(boost::asio::buffer(received_),
			boost::bind(&tcp_provider_impl::connection::received, shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
}

void
tcp_provider_impl::connection::send_queued() {
	if (owner_->has_enabled_magic_cookies_)
		send_magic_cookie();

	std::deque< std::vector< uint8_t > > &current_queue
		= owner_->current_queue_->second;

	boost::asio::async_write(
			socket_,
			boost::asio::buffer(&current_queue.front()[0],
								current_queue.front().size()),
			boost::bind(&provider_base_impl::sent, owner_,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
}

const uint8_t *
tcp_provider_impl::connection::get_received_data() const {
	return received_.data();
}

void tcp_provider_impl::connection::send_magic_cookie() {
	static uint8_t magic_cookie[] = { 0xFF, 0xFF, 0x80, 0x00,
									   0x00, 0x00, 0x00, 0x08,
									   0xDE, 0xAD, 0xBE, 0xEF,
									   0x01, 0x01, 0x02, 0x00 };
}

void
tcp_provider_impl::connection::received(
		boost::system::error_code const &_error_code, std::size_t _transferred_bytes) {
	owner_->current_receiving_ = this;
	owner_->received(_error_code, _transferred_bytes);
}

} // namespace vsomeip
