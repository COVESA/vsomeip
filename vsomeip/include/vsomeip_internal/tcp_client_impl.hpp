//
// tcp_client_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright 2013, 2024 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_TCP_CLIENT_IMPL_HPP
#define VSOMEIP_INTERNAL_TCP_CLIENT_IMPL_HPP

#include <boost/asio/ip/tcp.hpp>

#include <vsomeip_internal/config.hpp>
#include <vsomeip_internal/client_impl.hpp>

namespace vsomeip {

class endpoint;

typedef client_impl<boost::asio::ip::tcp,
					 VSOMEIP_MAX_TCP_MESSAGE_SIZE> tcp_client_base_impl;

class tcp_client_impl
	: public tcp_client_base_impl {
public:
	tcp_client_impl(
			boost::asio::io_service &_service, const endpoint *_location);
	virtual ~tcp_client_impl();

	void start();
	void send_queued();

private:
	void send_magic_cookie();

	void connect();
	void receive();
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_TCP_CLIENT_IMPL_HPP
