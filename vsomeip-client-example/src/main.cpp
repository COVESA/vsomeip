//
// main.cpp
//
// Date: 	Jan 16, 2014
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//
#include <iostream>
#include <vsomeip/vsomeip.hpp>

int main(int argc, char **argv) {

	vsomeip::factory * default_factory = vsomeip::factory::get_default_factory();
	vsomeip::endpoint * target = default_factory->create_endpoint();
	target->set_protocol(vsomeip::ip_protocol::UDP);
	target->set_version(vsomeip::ip_version::V4);
	target->set_address("127.0.0.1");
	target->set_port(3333);

	vsomeip::client *c = vsomeip::factory::get_default_factory()->create_client(*target);
	c->connect();

	vsomeip::message *test_message = default_factory->create_message();
	vsomeip::payload &test_payload = test_message->get_payload();
	uint8_t test_data[] = {  0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	test_payload.set_data(test_data, sizeof(test_data));

	std::cout << "SEND ONCE WITH FLUSH" << std::endl;
	c->send(*test_message);

	std::cout << "SEND FOUR TIME WITHOUT FOLLOWED BY ONCE WITH FLUSH" << std::endl;
	c->send(*test_message, false);
	c->send(*test_message, false);
	c->send(*test_message, false);
	c->send(*test_message, false);
	c->send(*test_message);

	std::cout << "SEND 54 TIMES TO OVERRUN THE MAXIMUM UDP MESSAGE SIZE" << std::endl;
	for (int i = 0; i < 54; i++)
		c->send(*test_message, false);
	c->send(*test_message);

	c->poll_one();
	c->poll_one();
	c->poll_one();
	c->poll_one();
	c->poll_one();
	c->poll_one();
	return 0;
}



