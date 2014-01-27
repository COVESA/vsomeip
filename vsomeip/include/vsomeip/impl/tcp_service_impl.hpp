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

#ifndef VSOMEIP_IMPL_TCP_SERVICE_IMPL_HPP
#define VSOMEIP_IMPL_TCP_SERVICE_IMPL_HPP

#include <boost/array.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <vsomeip/config.hpp>
#include <vsomeip/impl/service_base_impl.hpp>

namespace vsomeip {

class tcp_service_impl
		: virtual public service_base_impl {
public:
	tcp_service_impl(const endpoint *_endpoint);
	virtual ~tcp_service_impl();

	void start();
	void stop();

protected:
	boost::asio::ip::tcp::acceptor acceptor_;

private:
	void restart();
	void send_queued();

	std::string get_remote_address() const;
	uint16_t get_remote_port() const;
	ip_protocol get_protocol() const;
	ip_version get_version() const;

	const uint8_t * get_received() const;

	bool is_magic_cookie(const message_base *_message) const;

private:
	class connection : public boost::enable_shared_from_this< connection > {
	public:
		typedef boost::shared_ptr<connection> pointer;

		static pointer create(tcp_service_impl *_service);
		boost::asio::ip::tcp::socket & get_socket();
		void start();
		const uint8_t * get_received_data() const;

	private:
		connection(tcp_service_impl *_service);
		void send_magic_cookie();

		// buffer
		boost::array< uint8_t, VSOMEIP_MAX_TCP_MESSAGE_SIZE > received_;
		boost::asio::ip::tcp::socket socket_;

		tcp_service_impl * service_;

	private:
		void sent(boost::system::error_code const &_error_code,
				   std::size_t _transferred_bytes);
		void received(boost::system::error_code const &_error_code,
						std::size_t _transferred_bytes);
	};

	// active connections
	std::map< endpoint *, connection> connections_;

	// current receive buffer
	connection *current_receiving_;

	// IP version used for this service
	ip_version version_;

	void accepted(connection::pointer _connection,
		  boost::system::error_code const &_error_code);

};

} // namespace vsomeip

#endif // VSOMEIP_IMPL_TCP_SERVICE_IMPL_HPP
