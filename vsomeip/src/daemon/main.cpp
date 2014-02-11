//
// main.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/service_discovery/internal/daemon.hpp>

int
main(int argc, char **argv) {

	vsomeip::service_discovery::daemon *the_daemon
		= vsomeip::service_discovery::daemon::get_instance();

	// create argument vector and pass it to the daemon object
	the_daemon->init(argc, argv);

	// start the daemon
	the_daemon->start();
}


