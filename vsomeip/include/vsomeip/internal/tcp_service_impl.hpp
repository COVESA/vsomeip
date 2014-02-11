//
// tcp_service_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_TCP_SERVICE_IMPL_HPP
#define VSOMEIP_INTERNAL_TCP_SERVICE_IMPL_HPP

#include <boost/array.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <vsomeip/config.hpp>
#include <vsomeip/internal/service_base_impl.hpp>

namespace vsomeip {

class tcp_service_impl
		: virtual public service_base_impl {
public:
	tcp_service_impl(
			const factory *_factory,
			const endpoint *_endpoint,
			boost::asio::io_service &_is);
	virtual ~tcp_service_impl();

	void start();
	void stop();

protected:
	boost::asio::ip::tcp::acceptor acceptor_;

private:
	void restart();
	void send_queued();

	ip_address get_remote_address() const;
	ip_port get_remote_port() const;
	ip_protocol get_protocol() const;
	ip_version get_version() const;

	const uint8_t * get_received() const;

	bool is_magic_cookie(
			message_id _message_id, length _length, request_id _request_id,
			protocol_version _protocol_version, interface_version _interface_version,
			message_type _message_type, return_code _return_code) const;

private:
	class connection : public boost::enable_shared_from_this< connection > {
	public:
		typedef boost::shared_ptr<connection> pointer;

		static pointer create(tcp_service_impl *_service);
		boost::asio::ip::tcp::socket & get_socket();
		void start();
		void send_queued();
		const uint8_t * get_received_data() const;

	private:
		connection(tcp_service_impl *_service);

		void send_magic_cookie();

		// buffer
		boost::array< uint8_t, VSOMEIP_MAX_TCP_MESSAGE_SIZE > received_;
		boost::asio::ip::tcp::socket socket_;

		tcp_service_impl * service_;

	private:
		void received(boost::system::error_code const &_error_code,
						std::size_t _transferred_bytes);
	};

	// active connections
	std::map< endpoint *, connection::pointer > connections_;

	// current receive buffer
	connection *current_receiving_;

	// IP version used for this service
	ip_version version_;

	void accepted(connection::pointer _connection,
		  boost::system::error_code const &_error_code);

};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_TCP_SERVICE_IMPL_HPP
