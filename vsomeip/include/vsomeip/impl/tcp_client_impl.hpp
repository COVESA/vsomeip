//
// tcp_client_impl.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_IMPL_TCP_CLIENT_IMPL_HPP
#define VSOMEIP_IMPL_TCP_CLIENT_IMPL_HPP

#include <boost/array.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <vsomeip/config.hpp>
#include <vsomeip/impl/client_base_impl.hpp>

namespace vsomeip {

class endpoint;

class tcp_client_impl
		: virtual public client_base_impl {
public:
	tcp_client_impl(const endpoint *_endpoint);

	void start();
	void stop();

private:
	void restart();
	void send_queued();

	std::string get_remote_address() const;
	uint16_t get_remote_port() const;
	ip_protocol get_protocol() const;
	ip_version get_version() const;

	const uint8_t * get_received() const;

private:
	boost::asio::ip::tcp::socket socket_;
	boost::asio::ip::tcp::endpoint local_endpoint_;

	boost::array< uint8_t, VSOMEIP_MAX_TCP_MESSAGE_SIZE > received_;
};

} // namespace vsomeip

#endif // VSOMEIP_IMPL_TCP_CLIENT_IMPL_HPP
