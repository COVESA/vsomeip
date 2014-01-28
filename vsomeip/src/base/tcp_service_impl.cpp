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
#include <vsomeip/internal/tcp_service_impl.hpp>

namespace ip = boost::asio::ip;

namespace vsomeip {

tcp_service_impl::tcp_service_impl(const endpoint *_endpoint)
	: service_base_impl(VSOMEIP_MAX_TCP_MESSAGE_SIZE),
	  acceptor_(is_,
			    ip::tcp::endpoint((_endpoint->get_version() == ip_version::V6 ?
			    				  	  ip::tcp::v6() : ip::tcp::v4()),
			    				  _endpoint->get_port())),
	  version_(_endpoint->get_version()) {

	has_magic_cookies_ = true;
}

tcp_service_impl::~tcp_service_impl() {
}

void tcp_service_impl::start() {
	connection::pointer new_connection = connection::create(this);

	acceptor_.async_accept(new_connection->get_socket(),
			boost::bind(&tcp_service_impl::accepted, this,
						new_connection, boost::asio::placeholders::error));
}

void tcp_service_impl::restart() {
	if (current_receiving_)
		current_receiving_->start();
}

void tcp_service_impl::stop() {
}

void tcp_service_impl::send_queued() {

}


std::string tcp_service_impl::get_remote_address() const {
	return (current_receiving_ == 0 ?
				0 : current_receiving_->get_socket().
						remote_endpoint().address().to_string());
}

uint16_t tcp_service_impl::get_remote_port() const {
	return (current_receiving_ == 0 ?
				0 : current_receiving_->get_socket().remote_endpoint().port());
}

ip_protocol tcp_service_impl::get_protocol() const {
	return ip_protocol::TCP;
}

ip_version tcp_service_impl::get_version() const {
	return version_;
}

const uint8_t * tcp_service_impl::get_received() const {
	return (current_receiving_ == 0 ?
				0 : current_receiving_->get_received_data());
}

bool tcp_service_impl::is_magic_cookie(
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

void tcp_service_impl::accepted(
		connection::pointer _connection,
		const boost::system::error_code& _error_code) {

	if (!_error_code) {
		_connection->start();
	}

	start();
}

// Inner class connection
tcp_service_impl::connection::connection(tcp_service_impl *_service)
	: socket_(_service->is_) {
	service_ = _service;
}

tcp_service_impl::connection::pointer
tcp_service_impl::connection::create(tcp_service_impl *_service) {
	return pointer(new connection(_service));
}

boost::asio::ip::tcp::socket &
tcp_service_impl::connection::get_socket() {
	return socket_;
}

void
tcp_service_impl::connection::start() {
	socket_.async_receive(boost::asio::buffer(received_),
			boost::bind(&tcp_service_impl::connection::received, shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
}

const uint8_t *
tcp_service_impl::connection::get_received_data() const {
	return received_.data();
}

#ifdef USE_VSOMEIP_MAGIC_COOKIES
void tcp_service_impl::connection::send_magic_cookie() {
	static uint8_t magic_cookie[] = { 0xFF, 0xFF, 0x80, 0x00,
									   0x00, 0x00, 0x00, 0x08,
									   0xDE, 0xAD, 0xBE, 0xEF,
									   0x01, 0x01, 0x02, 0x00 };
}
#endif

void
tcp_service_impl::connection::sent(
		boost::system::error_code const &_error_code, std::size_t _transferred_bytes) {
}

void
tcp_service_impl::connection::received(
		boost::system::error_code const &_error_code, std::size_t _transferred_bytes) {
	service_->current_receiving_ = this;
	service_->received(_error_code, _transferred_bytes);
}

} // namespace vsomeip
