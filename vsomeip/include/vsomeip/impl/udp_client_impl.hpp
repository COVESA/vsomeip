//
// client_impl.hpp
//
// Date: 	Jan 14, 2014
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_IMPL_CLIENT_IMPL_HPP
#define VSOMEIP_IMPL_CLIENT_IMPL_HPP

#include <deque>
#include <set>
#include <vector>

#include <boost/asio.hpp>

#include <vsomeip/config.hpp>
#include <vsomeip/client.hpp>
#include <vsomeip/impl/send_buffer_impl.hpp>

namespace vsomeip {

class udp_client_impl: virtual public client {
public:
	udp_client_impl(const endpoint &_endpoint);
	virtual ~udp_client_impl();

	void open();
	void close();

	void connect();
	void disconnect();

	void send(const message &_message, bool _flush);

	void register_receiver(receiver *_receiver);
	void unregister_receiver(receiver *_receiver);

	size_t poll_one();
	size_t poll();
	size_t run();

private:
	boost::asio::io_service io_;
	boost::asio::ip::udp::socket socket_;
	boost::asio::ip::udp::endpoint endpoint_;
	boost::asio::ip::udp version_;

	std::set< receiver *> receiver_;

	// message serialization/deserialization
	serializer *serializer_;
	deserializer *deserializer_;

	// buffers for sending messages
	std::deque< std::vector< uint8_t > > queue_;
	std::vector< uint8_t > current_send_buffer_;

private:
	void send();

	void connect_callback(boost::system::error_code const &error);
	void send_callback(boost::system::error_code const &error, std::size_t transferred_bytes);
	void receive_callback(boost::system::error_code const &error, std::size_t transferred_bytes);
};

} // namespace vsomeip

#endif // VSOMEIP_IMPL_CLIENT_IMPL_HPP
