//
// main.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#include <iostream>

#include <vsomeip/vsomeip.hpp>
#include <vsomeip/service_discovery/factory.hpp>
#include <vsomeip/service_discovery/client.hpp>

int main(int argc, char **argv) {

	vsomeip::service_discovery::factory *default_factory
		= vsomeip::service_discovery::factory::get_default_factory();

	vsomeip::service_discovery::client *the_client
		= default_factory->create_service_discovery_client();

	vsomeip::consumer *the_consumer	= the_client->create_consumer(0x2233, 0x0012);
	the_consumer->start();

	//the_client->process();
	the_client->run();
	return 0;
}
