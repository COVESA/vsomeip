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
#define SERVICE_PORT VSOMEIP_LOWEST_VALID_PORT

int main(int argc, char **argv) {
	vsomeip::factory *the_factory = vsomeip::factory::get_default_factory();
	vsomeip::endpoint *the_endpoint = the_factory->get_endpoint(
										SERVICE_HOST, SERVICE_PORT,
										vsomeip::ip_protocol::TCP,
										vsomeip::ip_version::V4);
	vsomeip::client *the_client = the_factory->create_client(the_endpoint);

	// switch on magic cookies
	the_client->enable_magic_cookies();

	// create a wrong message
	uint8_t the_correct_message_payload[]
	    = { 0x11, 0x12, 0x22, 0x22,
	    	0x00, 0x00, 0x00, 0x11,
	    	0x22, 0x33, 0x44, 0x55,
	    	0x01, 0x01, 0x02, 0x00,
	    	0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01 };
	uint8_t the_wrong_message_payload[]
	    = { 0x11, 0x12, 0x22, 0x22,
	    	0x00, 0x00, 0x01, 0x23,
	    	0x22, 0x33, 0x44, 0x55,
	    	0x01, 0x01, 0x02, 0x00,
	    	0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01 };

	the_client->start();
	the_client->send(the_wrong_message_payload, sizeof(the_wrong_message_payload));
	the_client->send(the_correct_message_payload, sizeof(the_correct_message_payload));
	the_client->send(the_correct_message_payload, sizeof(the_correct_message_payload));
	the_client->send(the_wrong_message_payload, sizeof(the_wrong_message_payload));
	the_client->send(the_wrong_message_payload, sizeof(the_wrong_message_payload));
	the_client->send(the_wrong_message_payload, sizeof(the_wrong_message_payload));
	the_client->send(the_correct_message_payload, sizeof(the_correct_message_payload));
	the_client->run();

	while (1); // barrier

	return 0;
}

