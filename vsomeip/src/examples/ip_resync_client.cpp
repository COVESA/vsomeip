//
// ip_resync_client.cpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/vsomeip.hpp>

#define SERVICE_HOST "127.0.0.1"
#define SERVICE_PORT VSOMEIP_FIRST_VALID_PORT

int main(int argc, char **argv) {
	vsomeip::factory *the_factory = vsomeip::factory::get_default_factory();
	vsomeip::endpoint *the_endpoint = the_factory->create_endpoint(
										SERVICE_HOST, SERVICE_PORT,
										vsomeip::ip_protocol::TCP,
										vsomeip::ip_version::V4);
	vsomeip::client *the_client = the_factory->create_client(the_endpoint);

	// switch on magic cookies
	the_client->set_sending_magic_cookies(true);

	// create a wrong message
	vsomeip::message *the_message = the_factory->create_message();
	uint8_t the_message_payload[]
	    = { 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1 };
	the_message->get_payload().set_data(
		the_message_payload, sizeof(the_message_payload));
	the_client->start();

	the_message->set_length(51);
	the_client->send(the_message);
	the_message->set_length(17);
	the_client->send(the_message);
	the_client->run();

	while (1); // barrier

	return 0;
}

