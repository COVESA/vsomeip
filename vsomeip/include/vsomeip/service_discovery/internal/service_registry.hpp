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

#include <vsomeip/service_discovery/internal/daemon-config.hpp>

namespace vsomeip {
namespace service_discovery {

class daemon;
class service_administrator;

class service_registry
{
public:
	class session
		: public boost::enable_shared_from_this<session> {
	public:
		session(service_registry *_registry, boost::asio::io_service &_is);
		boost::asio::local::stream_protocol::socket & get_socket();
		void start();
		void receive(const boost::system::error_code &_error,
					   size_t _transferred_bytes);
		void consume_message();

	private:
		boost::asio::local::stream_protocol::socket socket_;
		boost::array<uint8_t, VSOMEIP_MAX_COMMAND_SIZE> data_;
		std::vector<uint8_t> message_;
		service_registry *registry_;
	};

	typedef boost::shared_ptr<session> session_ptr;

public:
	service_registry(daemon *_daemon);
	void handle_accept(session_ptr _session,
						   const boost::system::error_code &_error);

	service_administrator * find(service_id _service, instance_id _instance);
	service_administrator * add(service_id _service, instance_id _instance);
	void remove(service_id _service, instance_id _instance);

private:
	std::map<service_id,
			 std::map<instance_id,
			          service_administrator *>> data_;

	boost::asio::io_service &is_;
	boost::asio::local::stream_protocol::acceptor acceptor_;

	daemon *daemon_;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_INSTANCE_HPP
