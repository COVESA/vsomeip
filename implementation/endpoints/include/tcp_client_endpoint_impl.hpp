// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_TCP_CLIENT_ENDPOINT_IMPL_HPP
#define VSOMEIP_TCP_CLIENT_ENDPOINT_IMPL_HPP

#include <boost/asio/ip/tcp.hpp>

#include <vsomeip/defines.hpp>
#include "client_endpoint_impl.hpp"

namespace vsomeip {

typedef client_endpoint_impl<
			boost::asio::ip::tcp,
			VSOMEIP_MAX_TCP_MESSAGE_SIZE > tcp_client_endpoint_base_impl;

class tcp_client_endpoint_impl
	: public tcp_client_endpoint_base_impl {
public:
	tcp_client_endpoint_impl(std::shared_ptr< endpoint_host > _host,
			endpoint_type _local, boost::asio::io_service &_io);
	virtual ~tcp_client_endpoint_impl();

	void start();
	void send_queued(message_buffer_ptr_t _buffer);

	void join(const std::string &);
	void leave(const std::string &);

private:
	void send_magic_cookie(message_buffer_ptr_t &_buffer);

	void connect();
	void receive();
};

} // namespace vsomeip

#endif // VSOMEIP_TCP_CLIENT_ENDPOINT_IMPL_HPP
