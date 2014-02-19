//
// service_registration.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_REGISTRATION_HPP
#define VSOMEIP_SERVICE_REGISTRATION_HPP

#include <boost/array.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <vsomeip/service_discovery/internal/service_info.hpp>

namespace vsomeip {
namespace service_discovery {

class service_registry
{
public:
	class session
		: public boost::enable_shared_from_this<session> {
	public:
		session(boost::asio::io_service &_is);
		boost::asio::local::stream_protocol::socket & get_socket();
		void start();
		void receive(const boost::system::error_code &_error,
					   size_t _transferred_bytes);
		void consume_message();

	private:
		boost::asio::local::stream_protocol::socket socket_;
		boost::array<uint8_t, 10> data_;
		std::vector<uint8_t> message_;
	};

	typedef boost::shared_ptr<session> session_ptr;

public:
	service_registry(boost::asio::io_service &_is,
			const std::string &_location);
	void handle_accept(session_ptr _session,
						   const boost::system::error_code &_error);

	service_info * add(service_id _service, instance_id _instance);
	void remove(service_id _service, instance_id _instance);
	service_info * find(service_id _service, instance_id _instance);

private:
	std::map<service_id,
			 std::map<instance_id,
			          service_info>> data_;

	boost::asio::io_service &is_;
	boost::asio::local::stream_protocol::acceptor acceptor_;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_INSTANCE_HPP
