// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <sstream>

#include <boost/asio/write.hpp>

#include <vsomeip/defines.hpp>
#include <vsomeip/logger.hpp>

#include "../include/endpoint_host.hpp"
#include "../include/local_client_endpoint_impl.hpp"

namespace vsomeip {

local_client_endpoint_impl::local_client_endpoint_impl(
		std::shared_ptr< endpoint_host > _host, endpoint_type _remote, boost::asio::io_service &_io)
	: local_client_endpoint_base_impl(_host, _remote, _io) {
	is_supporting_magic_cookies_ = false;
}

void local_client_endpoint_impl::start() {
	connect();
}

void local_client_endpoint_impl::connect() {
	socket_.open(remote_.protocol());

	socket_.async_connect(
		remote_,
		std::bind(
			&local_client_endpoint_base_impl::connect_cbk,
			shared_from_this(),
			std::placeholders::_1
		)
	);
}

void local_client_endpoint_impl::receive() {
	std::shared_ptr< buffer_t > its_data
		= std::make_shared< buffer_t >(VSOMEIP_MAX_LOCAL_MESSAGE_SIZE);
	socket_.async_receive(
		boost::asio::buffer(*its_data),
		std::bind(
			&local_client_endpoint_impl::receive_cbk,
			std::dynamic_pointer_cast< local_client_endpoint_impl >(shared_from_this()),
			its_data,
			std::placeholders::_1,
			std::placeholders::_2
		)
	);
}

void local_client_endpoint_impl::send_queued(buffer_ptr_t _buffer) {
#if 0
	std::stringstream msg;
	msg << "lce<" << this << ">::sq: ";
	for (std::size_t i = 0; i < _data->size(); i++)
		msg << std::setw(2) << std::setfill('0') << std::hex << (int)(*_buffer)[i] << " ";
	msg << std::endl;
#endif

	static byte_t its_start_tag[] = { 0x67, 0x37, 0x6D, 0x07 };
	static byte_t its_end_tag[] = { 0x07, 0x6D, 0x37, 0x67 };

	boost::asio::async_write(
		socket_,
		boost::asio::buffer(
			its_start_tag,
			sizeof(its_start_tag)
		),
		std::bind(
			&local_client_endpoint_impl::send_tag_cbk,
			std::dynamic_pointer_cast< local_client_endpoint_impl >(shared_from_this()),
			std::placeholders::_1,
			std::placeholders::_2
		)
	);

	boost::asio::async_write(
		socket_,
		boost::asio::buffer(*_buffer),
		std::bind(
			&client_endpoint_impl::send_cbk,
			this->shared_from_this(),
			_buffer,
			std::placeholders::_1,
			std::placeholders::_2
		)
	);

	boost::asio::async_write(
		socket_,
		boost::asio::buffer(
			its_end_tag,
			sizeof(its_end_tag)
		),
		std::bind(
			&local_client_endpoint_impl::send_tag_cbk,
			std::dynamic_pointer_cast< local_client_endpoint_impl >(shared_from_this()),
			std::placeholders::_1,
			std::placeholders::_2
		)
	);
}

void local_client_endpoint_impl::send_magic_cookie() {
}

void local_client_endpoint_impl::join(const std::string &) {
}

void local_client_endpoint_impl::leave(const std::string &) {
}

void local_client_endpoint_impl::send_tag_cbk(
		boost::system::error_code const &_error, std::size_t _bytes) {
}

void local_client_endpoint_impl::receive_cbk(
		buffer_ptr_t _buffer,
		boost::system::error_code const &_error, std::size_t _bytes) {
	VSOMEIP_ERROR << "Local endpoints must not receive messages!";
}

} // namespace vsomeip
