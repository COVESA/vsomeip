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
#include <vsomeip/service_discovery/application.hpp>
#include <vsomeip/service_discovery/factory.hpp>
#include <vsomeip/service_discovery/service.hpp>

#define REGISTER_SERVICE(S, I) 	\
	std::cout << "Registered service instance (" << std::hex << S << ", " << I << ") " \
			  << (the_service->register_service(S, I) ? "succeeded." : "failed.") \
			  << std::endl;

int main(int argc, char **argv) {

	vsomeip::service_discovery::factory *default_factory
		= vsomeip::service_discovery::factory::get_default_factory();
	vsomeip::service_discovery::application *application
		= default_factory->create_service_discovery_application();

	vsomeip::endpoint *tcp_target
		= default_factory->get_endpoint("127.0.0.1", VSOMEIP_LOWEST_VALID_PORT,
										vsomeip::ip_protocol::TCP,
										vsomeip::ip_version::V4);

	vsomeip::service_discovery::service *the_service
		= application->create_service_discovery_service(0x1234, 0x678, tcp_target);

	REGISTER_SERVICE(0x2222, 0x3333);
	REGISTER_SERVICE(0x2222, 0x3334);
	REGISTER_SERVICE(0x2221, 0x3333);

	the_service->start();
	the_service->stop();

	while (1) application->run();
	return 0;
}
