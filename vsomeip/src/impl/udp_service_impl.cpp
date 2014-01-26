//
// udp_service_impl.cpp
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

#include <vsomeip/config.hpp>
#include <vsomeip/endpoint.hpp>
#include <vsomeip/serializer.hpp>
#include <vsomeip/impl/udp_service_impl.hpp>

namespace ip = boost::asio::ip;

namespace vsomeip {

udp_service_impl::udp_service_impl(const endpoint *_endpoint)
		: service_base_impl(VSOMEIP_MAX_UDP_MESSAGE_SIZE),
		  socket_(is_, ip::udp::endpoint((_endpoint->get_version() == ip_version::V4 ? ip::udp::v4() : ip::udp::v6()),
				  		  _endpoint->get_port())) {
}

void udp_service_impl::start() {
	socket_.async_receive_from(
	        boost::asio::buffer(received_), remote_endpoint_,
	        boost::bind(&participant_impl::received, this,
	          boost::asio::placeholders::error,
	          boost::asio::placeholders::bytes_transferred));
}

void udp_service_impl::restart() {
	start();
}

void udp_service_impl::stop() {
}

void udp_service_impl::send_queued() {
	ip::udp::endpoint target(
			ip::address::from_string(current_queue_->first->get_address()),
	  		current_queue_->first->get_port());

	socket_.async_send_to(
		boost::asio::buffer(&current_queue_->second.front()[0],
							current_queue_->second.front().size()),
		target,
		boost::bind(&service_base_impl::sent, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
}

std::string udp_service_impl::get_remote_address() const {
	return remote_endpoint_.address().to_string();
}

uint16_t udp_service_impl::get_remote_port() const {
	return remote_endpoint_.port();
}

ip_protocol udp_service_impl::get_protocol() const {
	return ip_protocol::UDP;
}

ip_version udp_service_impl::get_version() const {
	return (remote_endpoint_.protocol().v4() == ip::udp::v4() ?
				ip_version::V4 : ip_version::V6);
}

const uint8_t * udp_service_impl::get_received() const {
	return received_.data();
}

} // namespace vsomeip
