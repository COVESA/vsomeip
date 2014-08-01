// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_LOCAL_CLIENT_ENDPOINT_IMPL_HPP
#define VSOMEIP_LOCAL_CLIENT_ENDPOINT_IMPL_HPP

#include <boost/asio/io_service.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include <vsomeip/defines.hpp>

#include "client_endpoint_impl.hpp"

namespace vsomeip {

typedef client_endpoint_impl<
			boost::asio::local::stream_protocol,
			VSOMEIP_MAX_LOCAL_MESSAGE_SIZE > local_client_endpoint_base_impl;

class local_client_endpoint_impl
	: public local_client_endpoint_base_impl {
public:
	local_client_endpoint_impl(std::shared_ptr< endpoint_host > _host,
			endpoint_type _local, boost::asio::io_service &_io);

	void start();

	void send_queued(message_buffer_ptr_t _data);

private:
	void send_magic_cookie();

	void connect();
	void receive();

	void send_tag_cbk(boost::system::error_code const &_error, std::size_t _bytes);
	void receive_cbk(packet_buffer_ptr_t _buffer,
			boost::system::error_code const &_error, std::size_t _bytes);
};

} // namespace vsomeip

#endif // VSOMEIP_LOCAL_CLIENT_ENDPOINT_IMPL_HPP
