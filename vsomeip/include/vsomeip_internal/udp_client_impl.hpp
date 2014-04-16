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

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>

#include <vsomeip/config.hpp>
#include <vsomeip_internal/client_impl.hpp>

namespace vsomeip {

class endpoint;

typedef client_impl< boost::asio::ip::udp,
					  VSOMEIP_MAX_UDP_MESSAGE_SIZE > udp_client_base_impl;

class udp_client_impl
	: virtual public udp_client_base_impl {

public:
	udp_client_impl(
			managing_application_impl *_owner, const endpoint *_location);
	virtual ~udp_client_impl();

	void start();
	void send_queued();

	ip_address get_remote_address() const;
	ip_port get_remote_port() const;
	ip_protocol get_protocol() const;

private:
	void connect();
	void receive();

private:
	endpoint_type remote_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_UDP_CLIENT_IMPL_HPP
