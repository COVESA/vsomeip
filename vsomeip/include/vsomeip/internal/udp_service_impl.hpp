//
// udp_service_impl.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_UDP_SERVICE_IMPL_HPP
#define VSOMEIP_INTERNAL_UDP_SERVICE_IMPL_HPP

#include <boost/array.hpp>
#include <boost/asio/ip/udp.hpp>

#include <vsomeip/internal/service_base_impl.hpp>

namespace vsomeip {

class endpoint;

class udp_service_impl
		: virtual public service_base_impl {
public:
	udp_service_impl(
			const factory *_factory,
			const endpoint *_endpoint,
			boost::asio::io_service &_is);
	virtual ~udp_service_impl();

	void start();
	void stop();

	void connect();
	void receive();

private:
	void restart();
	void send_queued();

	ip_address get_remote_address() const;
	ip_port get_remote_port() const;
	ip_protocol get_protocol() const;
	ip_version get_version() const;

	const uint8_t * get_received() const;

private:
	boost::asio::ip::udp::socket socket_;
	boost::asio::ip::udp::endpoint remote_endpoint_;
	boost::array< uint8_t, VSOMEIP_MAX_UDP_MESSAGE_SIZE > received_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_UDP_SERVICE_IMPL_HPP
