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

#include "daemon.h"

int
main(int argc, char **argv) {

	vsomeip::Daemon* l_daemon = vsomeip::Daemon::getInstance();

	// create argument vector and pass it to the daemon object
	l_daemon->init(argc, argv);

	// start the daemon
	l_daemon->start();
}


