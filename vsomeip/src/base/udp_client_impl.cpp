//
// udp_client_impl.cpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <boost/asio/placeholders.hpp>
#include <boost/bind.hpp>

#include <vsomeip/endpoint.hpp>
#include <vsomeip/config.hpp>
#include <vsomeip/internal/udp_client_impl.hpp>

namespace ip = boost::asio::ip;

namespace vsomeip {

udp_client_impl::udp_client_impl(
		const factory *_factory,
		const endpoint *_endpoint,
		boost::asio::io_service &_is)
		: client_base_impl(_factory, VSOMEIP_MAX_UDP_MESSAGE_SIZE, _is),
		  socket_(is_),
		  local_endpoint_(ip::address::from_string(_endpoint->get_address()),
				  		  _endpoint->get_port()) {
}

udp_client_impl::~udp_client_impl() {
}

void udp_client_impl::start() {
	socket_.open(local_endpoint_.protocol().v4());
	connect();
	receive();
}

void udp_client_impl::connect() {
	socket_.async_connect(local_endpoint_,
			boost::bind(&client_base_impl::connected, this,
					boost::asio::placeholders::error));
}

void udp_client_impl::receive() {
	socket_.async_receive_from(
		        boost::asio::buffer(received_), remote_endpoint_,
		        boost::bind(&participant_impl::received, this,
		          boost::asio::placeholders::error,
		          boost::asio::placeholders::bytes_transferred));
}

void udp_client_impl::stop() {
	if (socket_.is_open())
		socket_.close();
}

void udp_client_impl::restart() {
	socket_.async_receive_from(
	        boost::asio::buffer(received_), remote_endpoint_,
	        boost::bind(&participant_impl::received, this,
	          boost::asio::placeholders::error,
	          boost::asio::placeholders::bytes_transferred));
}

void udp_client_impl::send_queued() {
	socket_.async_send(
		boost::asio::buffer(&packet_queue_.front()[0],
				packet_queue_.front().size()),
		boost::bind(&client_base_impl::sent, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
}

std::string udp_client_impl::get_remote_address() const {
	return remote_endpoint_.address().to_string();
}

uint16_t udp_client_impl::get_remote_port() const {
	return remote_endpoint_.port();
}

ip_protocol udp_client_impl::get_protocol() const {
	return ip_protocol::UDP;
}

ip_version udp_client_impl::get_version() const {
	return (remote_endpoint_.protocol().v4() == ip::udp::v4() ?
			ip_version::V4 : ip_version::V6);
}

const uint8_t * udp_client_impl::get_received() const {
	return received_.data();
}

} // namespace vsomeip





