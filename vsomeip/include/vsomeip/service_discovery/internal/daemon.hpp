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

#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {
namespace daemon {

class daemon {

public:
	static daemon * get_instance();

	void init(int a_argc, char **a_argv);
	void start();
	void stop();

private:
	daemon();
	void run();

private:
	int port_;
	bool is_service_discovery_mode_;
	bool is_virtual_mode_;

	boost::log::sources::severity_logger<
		boost::log::trivial::severity_level > log_;
	boost::log::trivial::severity_level loglevel_;
};

} // namespace daemon
} // namespace vsomeip

#endif // VSOMEIP_DAEMON_DAEMON_HPP
