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
#include <iostream>
#include <vsomeip/vsomeip.hpp>

int main(int argc, char **argv) {

	vsomeip::factory * default_factory = vsomeip::factory::get_default_factory();
	vsomeip::endpoint * target = default_factory->create_endpoint();
	target->set_protocol(vsomeip::ip_protocol::TCP);
	target->set_version(vsomeip::ip_version::V4);
	target->set_port(3333);

	vsomeip::service *c = vsomeip::factory::get_default_factory()->create_service(*target);
	c->start();
	c->run();

	return 0;
}



