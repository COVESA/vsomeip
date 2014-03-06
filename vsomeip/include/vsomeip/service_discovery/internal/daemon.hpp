//
// daemon.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef	 VSOMEIP_DAEMON_DAEMON_HPP
#define VSOMEIP_DAEMON_DAEMON_HPP

#include <boost/asio/io_service.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/endpoint.hpp>
#include <vsomeip/receiver.hpp>
#include <vsomeip/internal/loggable.hpp>
#include <vsomeip/service_discovery/service_entry.hpp>
#include <vsomeip/service_discovery/internal/service_info.hpp>
#include <vsomeip/service_discovery/internal/service_registry.hpp>

namespace vsomeip {

class provider;

namespace service_discovery {

class daemon : public receiver, public loggable {
public:
	static daemon * get_instance();

	void init(int a_argc, char **a_argv);
	void start();
	void stop();

	void receive(const message_base *_message);
	bool send(const message_base *_message);

	boost::asio::io_service & get_is();

	endpoint * get_multicast_address() const;

private:
	daemon();
	void run_service();
	void consume_request(const entry &, endpoint *);

private:
	service_registry *registry_;
	std::string registry_path_;

	vsomeip::provider *tcp_daemon_;
	vsomeip::provider *udp_daemon_;

	int port_;
	bool use_service_discovery_;
	bool use_virtual_mode_;
	bool is_running_;

	boost::asio::io_service is_;
	boost::asio::local::stream_protocol::acceptor acceptor_;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_DAEMON_DAEMON_HPP
