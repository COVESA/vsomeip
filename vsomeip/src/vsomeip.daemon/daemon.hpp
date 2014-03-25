//
// daemon.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_DAEMON_DAEMON_HPP
#define VSOMEIP_DAEMON_DAEMON_HPP

namespace vsomeip {

class endpoint;
class client;
class service;
class message_base;

class daemon {
public:
	static daemon * get_instance();

	virtual ~daemon() {};

	virtual void init(int _count, char **_options) = 0;
	virtual void start() = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_DAEMON_DAEMON_HPP
