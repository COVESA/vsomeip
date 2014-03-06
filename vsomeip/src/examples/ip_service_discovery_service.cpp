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
#include <vsomeip/service_discovery/client.hpp>
#include <vsomeip/service_discovery/factory.hpp>

#define REGISTER_SERVICE(S, I) 	\
	std::cout << "Registered service instance (" << std::hex << S << ", " << I << ") " \
			  << (the_provider->register_service(S, I) ? "succeeded." : "failed.") \
			  << std::endl;

int main(int argc, char **argv) {

	vsomeip::service_discovery::factory *default_factory
		= vsomeip::service_discovery::factory::get_default_factory();
	vsomeip::service_discovery::client *the_client
		= default_factory->create_service_discovery_client();

	vsomeip::endpoint *tcp_target
		= default_factory->get_endpoint("127.0.0.1", VSOMEIP_LOWEST_VALID_PORT,
										vsomeip::ip_protocol::TCP,
										vsomeip::ip_version::V4);

	vsomeip::provider *the_provider
		= the_client->create_provider(0x1234, 0x678, tcp_target);

	/*
	REGISTER_SERVICE(0x2222, 0x3333);
	REGISTER_SERVICE(0x2222, 0x3334);
	REGISTER_SERVICE(0x2221, 0x3333);
	REGISTER_SERVICE(0x2233, 0x0012);
	*/

	the_provider->start();
	the_provider->stop();

	while (1) the_client->run();
	return 0;
}
