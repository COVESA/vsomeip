//
// udp_service_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright 2013, 2024 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <iostream>

#include <boost/asio/placeholders.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/multicast.hpp>
#include <boost/bind.hpp>

#include <vsomeip/endpoint.hpp>
#include <vsomeip_internal/managing_proxy_impl.hpp>
#include <vsomeip_internal/udp_service_impl.hpp>

namespace ip = boost::asio::ip;

namespace vsomeip {

udp_service_impl::udp_service_impl(
		managing_proxy_impl *_owner, const endpoint *_location)
	: service_impl< ip::udp, VSOMEIP_MAX_UDP_MESSAGE_SIZE >(_owner, _location),
	  socket_(_owner->get_service(),
			  ip::udp::endpoint(
			      (_location->get_version() == ip_protocol_version::V4 ?
	  	  			  ip::udp::v4() : ip::udp::v6()),
	  	  		  _location->get_port()
	  	  	  )
      ) {

	boost::asio::socket_base::broadcast option(true);
	socket_.set_option(option);
}

udp_service_impl::~udp_service_impl() {
}

void udp_service_impl::start() {
	receive();
}

void udp_service_impl::stop() {
}

void udp_service_impl::send_queued() {
 	ip::udp::endpoint target(
		ip::address::from_string(
			current_queue_->first->get_address()
		),
		current_queue_->first->get_port()
	);

	socket_.async_send_to(
		boost::asio::buffer(
			&current_queue_->second.front()[0],
			current_queue_->second.front().size()
		),
		target,
		boost::bind(
			&udp_service_base_impl::send_cbk,
			this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred
		)
	);
}

void udp_service_impl::receive() {
	socket_.async_receive_from(
		boost::asio::buffer(buffer_),
		remote_,
		boost::bind(
			&participant_impl::receive_cbk,
			this,
	        boost::asio::placeholders::error,
	        boost::asio::placeholders::bytes_transferred
		)
	);
}

void udp_service_impl::restart() {
	receive();
}

ip_address udp_service_impl::get_remote_address() const {
	return remote_.address().to_string();
}

ip_port udp_service_impl::get_remote_port() const {
	return remote_.port();
}

ip_protocol udp_service_impl::get_protocol() const {
	return ip_protocol::UDP;
}

const uint8_t * udp_service_impl::get_buffer() const {
	return buffer_.data();
}

void udp_service_impl::join(const std::string &_multicast_address) {
	if (ip_protocol_version::V4 == location_->get_version()) {
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

void udp_service_impl::leave(const std::string &_multicast_address) {
	if (ip_protocol_version::V4 == location_->get_version()) {
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

} // namespace vsomeip

