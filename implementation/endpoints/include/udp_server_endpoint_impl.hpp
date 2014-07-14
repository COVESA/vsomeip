//
// udp_service_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright 2013, 2024 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_UDP_SERVICE_IMPL_HPP
#define VSOMEIP_INTERNAL_UDP_SERVICE_IMPL_HPP

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>

#include <vsomeip/defines.hpp>
#include "server_endpoint_impl.hpp"

namespace vsomeip {

typedef server_endpoint_impl<
			boost::asio::ip::udp,
			VSOMEIP_MAX_UDP_MESSAGE_SIZE > udp_server_endpoint_base_impl;

class udp_server_endpoint_impl
	: public udp_server_endpoint_base_impl {

public:
	udp_server_endpoint_impl(std::shared_ptr< endpoint_host > _host,
			endpoint_type _local, boost::asio::io_service &_io);
	virtual ~udp_server_endpoint_impl();

	void start();
	void stop();

	void restart();
	void receive();

	void send_queued(endpoint_type _target, message_buffer_ptr_t _buffer);
	endpoint_type get_remote() const;

	void join(const std::string &_multicast_address);
	void leave(const std::string &_multicast_address);

	bool is_v4() const;
	bool get_address(std::vector< byte_t > &_address) const;
	unsigned short get_port() const;
	bool is_udp() const;

public:
	void receive_cbk(packet_buffer_ptr_t _buffer,
			boost::system::error_code const &_error, std::size_t _size);

private:
	socket_type socket_;
	endpoint_type remote_;

	message_buffer_t message_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_UDP_SERVICE_IMPL_HPP
