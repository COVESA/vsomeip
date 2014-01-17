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
#include <vsomeip/config.hpp>
#include <vsomeip/service.hpp>
#include <vsomeip/impl/statistics_impl.hpp>

namespace vsomeip {

class serializer;
class deserializer;

class endpoint;

class receiver;

class tcp_service_impl : virtual public service {
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
	boost::asio::ip::tcp::socket socket_;
	boost::asio::ip::tcp::endpoint endpoint_;

	// registered receivers
	std::set< receiver *> receiver_;

	// buffer
	boost::array< uint8_t, VSOMEIP_MAX_UDP_MESSAGE_SIZE > buffer_;

	// message serialization/deserialization
	serializer *serializer_;
	deserializer *deserializer_;

	boost::asio::ip::tcp::endpoint remote_;

private:
	void send_callback(boost::system::error_code const &error,
					   std::size_t transferred_bytes);
	void receive_callback(boost::system::error_code const &error,
						  std::size_t transferred_bytes);

#ifdef USE_VSOMEIP_STATISTICS
private:
	statistics_impl statistics_;
public:
	const statistics * get_statistics() const;
#endif
};

} // namespace vsomeip

#endif // VSOMEIP_IMPL_TCP_SERVICE_IMPL_HPP
