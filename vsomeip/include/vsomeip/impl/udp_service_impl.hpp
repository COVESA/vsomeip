//
// udp_service_impl.hpp
//
// Date: 	Jan 15, 2014
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_IMPL_UDP_SERVICE_IMPL_HPP
#define VSOMEIP_IMPL_UDP_SERVICE_IMPL_HPP

#include <set>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <vsomeip/config.hpp>
#include <vsomeip/service.hpp>

namespace vsomeip {

class serializer;
class deserializer;

class endpoint;

class receiver;

class udp_service_impl : virtual public service {
public:
	udp_service_impl(const endpoint &_endpoint);
	virtual ~udp_service_impl();

	void start();
	void stop();

	std::size_t poll_one();
	std::size_t poll();
	std::size_t run();

protected:
	boost::asio::io_service io_;
	boost::asio::ip::udp::socket socket_;
	boost::asio::ip::udp::endpoint endpoint_;

	// registered receivers
	std::set< receiver *> receiver_;

	// buffer
	boost::array< uint8_t, VSOMEIP_MAX_UDP_MESSAGE_SIZE > buffer_;

	// message serialization/deserialization
	serializer *serializer_;
	deserializer *deserializer_;

	boost::asio::ip::udp::endpoint remote_;

private:
	void send_callback(boost::system::error_code const &error, std::size_t transferred_bytes);
	void receive_callback(boost::system::error_code const &error, std::size_t transferred_bytes);
};

} // namespace vsomeip

#endif // VSOMEIP_IMPL_UDP_SERVICE_IMPL_HPP
