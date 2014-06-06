//
// daemon.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_DAEMON_HPP
#define VSOMEIP_INTERNAL_DAEMON_HPP

#include <string>

#include <boost/asio/io_service.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint;
class client;
class service;
class message_base;

class daemon {
public:
	static daemon * get_instance();

	virtual ~daemon() {};

	virtual std::string get_name() const = 0;
	virtual boost::asio::io_service & get_service() = 0;
	virtual boost::log::sources::severity_logger< boost::log::trivial::severity_level > & get_logger() = 0;

	virtual void init(int _count, char **_options) = 0;
	virtual void start() = 0;
	virtual void stop() = 0;

	virtual bool send(message_base *_message, bool _reliable, bool _flush) = 0;

	virtual service * find_multicast_service(ip_port port) const = 0;

	virtual void on_service_availability(client_id _client, service_id _service, instance_id _instance, const endpoint *_location, bool _is_available) = 0;
	virtual void on_subscription(service_id _service, instance_id _instance, eventgroup_id _eventgroup, const endpoint *_location, bool _is_subscribing) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_DAEMON_HPP
