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

#include <vsomeip_internal/config.hpp>
#include <vsomeip_internal/service_impl.hpp>

namespace vsomeip {

class endpoint;

typedef service_impl<boost::asio::ip::udp,
					  VSOMEIP_MAX_UDP_MESSAGE_SIZE> udp_service_base_impl;

class udp_service_impl
	: public service_impl<boost::asio::ip::udp, VSOMEIP_MAX_UDP_MESSAGE_SIZE> {

public:
	udp_service_impl(
			boost::asio::io_service &_service, const endpoint *_location);
	virtual ~udp_service_impl();

	void start();
	void stop();

	void restart();
	void receive();
	void send_queued();

	const uint8_t * get_buffer() const;

private:
	buffer_type buffer_;
	socket_type socket_;
	endpoint_type remote_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_UDP_SERVICE_IMPL_HPP
