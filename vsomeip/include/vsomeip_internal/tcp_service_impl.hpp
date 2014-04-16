//
// tcp_service_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright 2013, 2024 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_TCP_SERVICE_IMPL_HPP
#define VSOMEIP_INTERNAL_TCP_SERVICE_IMPL_HPP

#include <map>

#include <boost/asio/ip/tcp.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <vsomeip/config.hpp>
#include <vsomeip_internal/service_impl.hpp>

namespace vsomeip {

class endpoint;

typedef service_impl< boost::asio::ip::tcp,
					   VSOMEIP_MAX_TCP_MESSAGE_SIZE > tcp_service_base_impl;

class tcp_service_impl
	: public service_impl< boost::asio::ip::tcp, VSOMEIP_MAX_TCP_MESSAGE_SIZE > {

public:
	tcp_service_impl(
			managing_application_impl *_owner, const endpoint *_location);
	virtual ~tcp_service_impl();

	void start();
	void stop();

	void send_queued();
	void restart();
	void receive();

	ip_address get_remote_address() const;
	ip_port get_remote_port() const;
	ip_protocol get_protocol() const;

	const uint8_t * get_buffer() const;

private:
	class connection
		: public boost::enable_shared_from_this<connection> {

	public:
		typedef boost::shared_ptr<connection> ptr;

		static ptr create(tcp_service_impl *_owner);
		socket_type & get_socket();

		void start();

		void send_queued();
		const uint8_t * get_buffer() const;

	private:
		connection(tcp_service_impl *_owner);
		void send_magic_cookie();

		tcp_service_impl::socket_type socket_;
		buffer_type buffer_;
		tcp_service_impl *owner_;

	private:
		void receive_cbk(
				boost::system::error_code const &_error, std::size_t _bytes);
	};

	boost::asio::ip::tcp::acceptor acceptor_;
	std::map< const endpoint *, connection::ptr > connections_;
	connection *current_;

private:
	void accept_cbk(
			connection::ptr _connection, boost::system::error_code const &_error);
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_TCP_SERVICE_IMPL_HPP
