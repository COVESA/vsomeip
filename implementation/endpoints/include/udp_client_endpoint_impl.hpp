//
// udp_client_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2024 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_UDP_CLIENT_IMPL_HPP
#define VSOMEIP_INTERNAL_UDP_CLIENT_IMPL_HPP

#include <memory>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>

#include <vsomeip/defines.hpp>

#include "client_endpoint_impl.hpp"

namespace vsomeip {

class endpoint_adapter;

typedef client_endpoint_impl<
			boost::asio::ip::udp,
			VSOMEIP_MAX_UDP_MESSAGE_SIZE > udp_client_endpoint_base_impl;

class udp_client_endpoint_impl
	: virtual public udp_client_endpoint_base_impl {

public:
	udp_client_endpoint_impl(std::shared_ptr< endpoint_host > _host,
			endpoint_type _remote, boost::asio::io_service &_io);
	virtual ~udp_client_endpoint_impl();

	void start();
	void send_queued(message_buffer_ptr_t _buffer);

	unsigned short get_port() const;

	void join(const std::string &_address);
	void leave(const std::string &_address);

	void receive_cbk(packet_buffer_ptr_t _buffer,
			boost::system::error_code const &_error, std::size_t _bytes);

private:
	void connect();
	void receive();
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_UDP_CLIENT_IMPL_HPP
