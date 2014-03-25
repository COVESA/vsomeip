//
// application_info.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_DAEMON_APPLICATION_INFO_HPP
#define VSOMEIP_DAEMON_APPLICATION_INFO_HPP

#include <map>
#include <set>

namespace vsomeip {

class endpoint;

struct application_info {

	// if watchdog_ is greater than VSOMEIP_MAX_MISSING_PONGS
	// the application is considered unavailable
	int8_t watchdog_;

	// queue the application wants to receive messages
	boost_ext::asio::message_queue *queue_;

	// Name to open the applications message queue. This name
	// can be provided to other application to enable p2p
	std::string queue_name_;

	// Services provided by the application
	std::map< service_id, std::map< instance_id, std::set< const endpoint * > > > services_;
};

} // namespace vsomeip

#endif // VSOMEIP_DAEMON_APPLICATION_INFO_HPP
