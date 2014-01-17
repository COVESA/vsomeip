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

#include <boost/date_time/posix_time/posix_time.hpp>

#include <vsomeip/vsomeip.hpp>

#define MESSAGE_PAYLOAD_SIZE 1374

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
	uint8_t test_data[MESSAGE_PAYLOAD_SIZE];
	for (int i = 0; i < MESSAGE_PAYLOAD_SIZE; ++i) test_data[i] = (i % 256);

	test_payload.set_data(test_data, sizeof(test_data));

	boost::posix_time::ptime t_start(boost::posix_time::microsec_clock::local_time());
	for (int i = 1; i <= 100000; i++) {
		// send directly
		c->send(*test_message);
	}
	c->run();
	boost::posix_time::ptime t_end(boost::posix_time::microsec_clock::local_time());


	const vsomeip::statistics * s = c->get_statistics();
	uint32_t sent_messages_count = 0;

	while (1) {
		if (sent_messages_count != s->get_sent_messages_count()) {
			sent_messages_count = s->get_sent_messages_count();
			std::cout << "Sent " << s->get_sent_messages_count() << " messages, containing "
								 << s->get_sent_bytes_count() << " bytes in "
								 << (t_end - t_start) << "s "
								 << "(" << (s->get_sent_messages_count() / (t_end - t_start).total_seconds()) << "/s) "
								 << "(" << (s->get_sent_bytes_count() / (t_end - t_start).total_seconds()) << "/s)"
								 << std::endl;
		}
		c->run();
	}

	return 0;
}



