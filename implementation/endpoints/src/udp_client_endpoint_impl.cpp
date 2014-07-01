// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <boost/asio/ip/multicast.hpp>

#include "../include/udp_client_endpoint_impl.hpp"

namespace vsomeip {

udp_client_endpoint_impl::udp_client_endpoint_impl(
		std::shared_ptr< endpoint_host > _host, endpoint_type _remote, boost::asio::io_service &_io)
	: udp_client_endpoint_base_impl(_host, _remote, _io) {
}

udp_client_endpoint_impl::~udp_client_endpoint_impl() {
}

//const endpoint * udp_client_endpoint_impl::get_local_endpoint() const {
//	return vsomeip::factory::get_instance()->get_endpoint(
//				socket_.local_endpoint().address().to_string(),
//				socket_.local_endpoint().port(),
//				ip_protocol::UDP
//		   );
//}

void udp_client_endpoint_impl::connect() {
	socket_.async_connect(
		remote_,
		std::bind(
			&udp_client_endpoint_base_impl::connect_cbk,
			shared_from_this(),
			std::placeholders::_1
		)
	);
}

void udp_client_endpoint_impl::start() {
	socket_.open(remote_.protocol());
	connect();
	receive();
}

void udp_client_endpoint_impl::send_queued() {
	socket_.async_send(
		boost::asio::buffer(
			&packet_queue_.front()[0],
			packet_queue_.front().size()
		),
		std::bind(
			&udp_client_endpoint_base_impl::send_cbk,
			shared_from_this(),
			std::placeholders::_1,
			std::placeholders::_2
		)
	);
}

void udp_client_endpoint_impl::receive() {
	socket_.async_receive_from(
		boost::asio::buffer(buffer_),
		remote_,
		std::bind(
			&udp_client_endpoint_base_impl::receive_cbk,
			shared_from_this(),
			std::placeholders::_1,
			std::placeholders::_2
		)
	);
}

void udp_client_endpoint_impl::join(const std::string &_multicast_address) {
	if (remote_.address().is_v4()) {
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

void udp_client_endpoint_impl::leave(const std::string &_multicast_address) {
	if (remote_.address().is_v4()) {
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

