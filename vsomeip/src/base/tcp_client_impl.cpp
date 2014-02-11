//
// tcp_client_impl.cpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <boost/asio/placeholders.hpp>
#include <boost/asio/write.hpp>
#include <boost/bind.hpp>

#include <vsomeip/config.hpp>
#include <vsomeip/constants.hpp>
#include <vsomeip/endpoint.hpp>
#include <vsomeip/internal/tcp_client_impl.hpp>

namespace ip = boost::asio::ip;

namespace vsomeip {

tcp_client_impl::tcp_client_impl(
		const factory *_factory,
		const endpoint *_endpoint,
		boost::asio::io_service &_is)
		: client_base_impl(_factory, VSOMEIP_MAX_TCP_MESSAGE_SIZE, _is),
		  socket_(is_),
		  local_endpoint_(ip::address::from_string(_endpoint->get_address()),
				  		  _endpoint->get_port()) {
	has_magic_cookies_ = true;
}

tcp_client_impl::~tcp_client_impl() {
}

void tcp_client_impl::start() {
	socket_.open(local_endpoint_.protocol().v4());
	socket_.async_connect(local_endpoint_,
			boost::bind(&client_base_impl::connected, this,
					boost::asio::placeholders::error));

	// Nagle algorithm off
	ip::tcp::no_delay option;
	socket_.set_option(option);

	socket_.async_receive(boost::asio::buffer(received_),
			boost::bind(&participant_impl::received, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
}

void tcp_client_impl::stop() {
	if (socket_.is_open())
		socket_.close();
}

void tcp_client_impl::restart() {
	socket_.async_receive(boost::asio::buffer(received_),
			boost::bind(&participant_impl::received, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
}

void tcp_client_impl::send_queued() {
	if (has_enabled_magic_cookies_)
		send_magic_cookie();

	boost::asio::async_write(
			socket_,
			boost::asio::buffer(&packet_queue_.front()[0],
								packet_queue_.front().size()),
			boost::bind(&client_base_impl::sent, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
}

ip_address tcp_client_impl::get_remote_address() const {
	return local_endpoint_.address().to_string();
}

ip_port tcp_client_impl::get_remote_port() const {
	return local_endpoint_.port();
}

ip_protocol tcp_client_impl::get_protocol() const {
	return ip_protocol::TCP;
}

ip_version tcp_client_impl::get_version() const {
	return (local_endpoint_.protocol().v4() == ip::tcp::v4() ?
			ip_version::V4 : ip_version::V6);
}

const uint8_t * tcp_client_impl::get_received() const {
	return received_.data();
}

bool tcp_client_impl::is_magic_cookie(
		message_id _message_id, length _length, request_id _request_id,
		protocol_version _protocol_version, interface_version _interface_version,
		message_type _message_type, return_code _return_code) const {
	return (_message_id	 == VSOMEIP_MAGIC_COOKIE_SERVICE_MESSAGE_ID &&
			 _length == VSOMEIP_MAGIC_COOKIE_LENGTH &&
			 _request_id == VSOMEIP_MAGIC_COOKIE_REQUEST_ID &&
			 _protocol_version == VSOMEIP_MAGIC_COOKIE_PROTOCOL_VERSION &&
			 _interface_version == VSOMEIP_MAGIC_COOKIE_INTERFACE_VERSION &&
			 _message_type == VSOMEIP_MAGIC_COOKIE_SERVICE_MESSAGE_TYPE &&
			 _return_code == VSOMEIP_MAGIC_COOKIE_RETURN_CODE);
}

void tcp_client_impl::send_magic_cookie() {
	static uint8_t magic_cookie_client_data[] = { 0xFF, 0xFF, 0x00, 0x00,
									   	    	   0x00, 0x00, 0x00, 0x08,
									   	    	   0xDE, 0xAD, 0xBE, 0xEF,
									   	    	   0x01, 0x01, 0x01, 0x00 };

	std::vector< uint8_t >& current_packet = packet_queue_.front();

	if (VSOMEIP_MAX_TCP_MESSAGE_SIZE - current_packet.size() >=
			VSOMEIP_STATIC_HEADER_LENGTH + VSOMEIP_MAGIC_COOKIE_LENGTH) {
		current_packet.insert(current_packet.begin(),
							  magic_cookie_client_data,
							  magic_cookie_client_data +
							  	  sizeof(magic_cookie_client_data));
	} else {
		//TODO: warn message: "Could not send magic cookie (no space left in packet)"
	}
}

} // namespace vsomeip





