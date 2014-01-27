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

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>

#include <vsomeip/vsomeip.hpp>

#define TCP_ENABLED 1
#define UDP_ENABLED 1

#define MESSAGE_PAYLOAD_SIZE 100
#define ITERATIONS 10

#ifdef TCP_ENABLED
vsomeip::client *tcp_client;
#endif

vsomeip::client *udp_client;

#ifdef TCP_ENABLED
void tcp_func() {
	tcp_client->run();
}
#endif

void udp_func() {
	udp_client->run();
}

int main(int argc, char **argv) {

	vsomeip::factory * default_factory = vsomeip::factory::get_default_factory();
#ifdef TCP_ENABLED
	vsomeip::endpoint * tcp_target
		= default_factory->create_endpoint("127.0.0.1", VSOMEIP_FIRST_VALID_PORT,
										   vsomeip::ip_protocol::TCP, vsomeip::ip_version::V4);
	tcp_client = vsomeip::factory::get_default_factory()->create_client(tcp_target);
	tcp_client->start();
#endif

	vsomeip::endpoint * udp_target
		= default_factory->create_endpoint("127.0.0.1", VSOMEIP_FIRST_VALID_PORT,
										   vsomeip::ip_protocol::UDP, vsomeip::ip_version::V4);
	udp_client = vsomeip::factory::get_default_factory()->create_client(udp_target);
	udp_client->start();

#ifdef TCP_ENABLED
	vsomeip::message *test_message = default_factory->create_message();
	test_message->set_method_id(0x2222);
	vsomeip::payload &test_payload = test_message->get_payload();
	uint8_t test_data[MESSAGE_PAYLOAD_SIZE];
	for (int i = 0; i < MESSAGE_PAYLOAD_SIZE; ++i) test_data[i] = (i % 256);
	test_payload.set_data(test_data, sizeof(test_data));
#endif

	vsomeip::message *test_message2 = default_factory->create_message();
	test_message2->set_service_id(0x1111);
	vsomeip::payload &test_payload2 = test_message2->get_payload();
	uint8_t test_data2[MESSAGE_PAYLOAD_SIZE];
	for (int i = 0; i < MESSAGE_PAYLOAD_SIZE; ++i) test_data2[i] = 0xFF;

	boost::posix_time::ptime t_start(boost::posix_time::microsec_clock::local_time());
	for (int i = 1; i <= ITERATIONS; i++) {
#ifdef TCP_ENABLED
		test_message->set_service_id(i % 2 == 0 ? 0x1111 : 0x1112);
		tcp_client->send(test_message);
#endif
		test_data2[0] = (i % 256);
		test_payload2.set_data(test_data2, sizeof(test_data2));
		test_message2->set_method_id(i % 2 == 0 ? 0x1222 : 0x2222);
		udp_client->send(test_message2, i == ITERATIONS);
	}
	boost::posix_time::ptime t_end(boost::posix_time::microsec_clock::local_time());

#ifdef TCP_ENABLED
	const vsomeip::statistics *tcp_statistics = tcp_client->get_statistics();
#endif
	const vsomeip::statistics *udp_statistics = udp_client->get_statistics();

#ifdef TCP_ENABLED
	boost::thread tcp_thread(tcp_func);
#endif
	boost::thread udp_thread(udp_func);

#ifdef TCP_ENABLED
	tcp_thread.join();
#endif
	udp_thread.join();

	int x = (t_end - t_start).total_seconds();
	if (x == 0) x = 1;

#ifdef TCP_ENABLED
	std::cout << "Sent " << tcp_statistics->get_sent_messages_count() << " TCP messages, containing "
						 << tcp_statistics->get_sent_bytes_count() << " bytes in "
						 << (t_end - t_start) << "s "
						 << "(" << (tcp_statistics->get_sent_messages_count() / x) << "/s) "
						 << "(" << (tcp_statistics->get_sent_bytes_count() / x) << "/s)"
						 << std::endl;
#endif

	std::cout << "Sent " << udp_statistics->get_sent_messages_count() << " UDP messages, containing "
						 << udp_statistics->get_sent_bytes_count() << " bytes in "
						 << (t_end - t_start) << "s "
						 << "(" << (udp_statistics->get_sent_messages_count() / x) << "/s) "
						 << "(" << (udp_statistics->get_sent_bytes_count() / x) << "/s)"
						 << std::endl;

	while (1); // barrier....
	return 0;
}
