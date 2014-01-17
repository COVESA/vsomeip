//
// udp_service_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_IMPL_TCP_SERVICE_IMPL_HPP
#define VSOMEIP_IMPL_TCP_SERVICE_IMPL_HPP

#include <set>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <vsomeip/config.hpp>
#include <vsomeip/service.hpp>
#ifdef USE_VSOMEIP_STATISTICS
#include <vsomeip/impl/statistics_owner_impl.hpp>
#endif

namespace vsomeip {

class serializer;
class deserializer;

class endpoint;

class receiver;

class tcp_service_impl : virtual public service
#ifdef USE_VSOMEIP_STATISTICS
, virtual public statistics_owner_impl
#endif
{
public:
	tcp_service_impl(const endpoint &_endpoint);
	virtual ~tcp_service_impl();

	void start();
	void stop();

	std::size_t poll_one();
	std::size_t poll();
	std::size_t run();

protected:
	boost::asio::io_service io_;
	boost::asio::ip::tcp::acceptor acceptor_;

	// registered receivers
	std::set< receiver *> receiver_;

private:
	class connection : public boost::enable_shared_from_this< connection > {
	public:
		typedef boost::shared_ptr<connection> pointer;

		static pointer create(boost::asio::io_service &_io);
		boost::asio::ip::tcp::socket & get_socket();
		void start();

	private:
		connection(boost::asio::io_service &_io);

		// buffer
		boost::array< uint8_t, VSOMEIP_MAX_UDP_MESSAGE_SIZE > buffer_;

		// message serialization/deserialization
		serializer *serializer_;
		deserializer *deserializer_;

		boost::asio::ip::tcp::socket socket_;
	private:
		void send_callback(boost::system::error_code const &error,
						   std::size_t transferred_bytes);
		void receive_callback(boost::system::error_code const &error,
							  std::size_t transferred_bytes);
	};

	void accept_callback(connection::pointer _connection, boost::system::error_code const &error);
};

} // namespace vsomeip

#endif // VSOMEIP_IMPL_TCP_SERVICE_IMPL_HPP
