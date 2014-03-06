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

#define FOREVER 1

#define TCP_ENABLED 1
#define UDP_ENABLED 1

#define TIMEOUT_BETWEEN 15
#define MESSAGE_PAYLOAD_SIZE 50
#define ITERATIONS 20

class mymessagereceiver : public vsomeip::receiver {
public:
	mymessagereceiver() : receive_counter(0), client_(0) {};

	void set_service(vsomeip::client *_client) { client_ = _client; };

	void receive(const vsomeip::message_base *_message) {
	}

	int receive_counter;
	vsomeip::client *client_;
};

#ifdef TCP_ENABLED
vsomeip::client *tcp_client;
vsomeip::consumer *tcp_consumer;
vsomeip::message *tcp_test_message;
mymessagereceiver tcp_receiver;
boost::mutex tcp_mutex;
#endif

#ifdef UDP_ENABLED
vsomeip::client *udp_client;
vsomeip::consumer *udp_consumer;
vsomeip::message *udp_test_message;
mymessagereceiver udp_receiver;
boost::mutex udp_mutex;
#endif


void send_tcp_messages(vsomeip::consumer *c, vsomeip::message *m, bool flush) {
	for (int i = 1; i <= ITERATIONS; i++) {
		tcp_mutex.lock();
		c->send(m, flush);
		tcp_mutex.unlock();
	}
}

void send_udp_messages(vsomeip::consumer *c, vsomeip::message *m, bool flush) {
	for (int i = 1; i <= ITERATIONS; i++) {
		udp_mutex.lock();
		c->send(m, flush);
		udp_mutex.unlock();
	}
}


#ifdef TCP_ENABLED
void tcp_poll() {
	while (FOREVER)
		tcp_client->poll();
}

void tcp_func(void *id) {
	int i = 1;
	while (FOREVER) {
		std::cout << "TCP Client " << id << ": run " << i++
				  << " sending " << ITERATIONS << " messages."
				  << std::endl;
		send_tcp_messages(tcp_consumer, tcp_test_message, true);
		boost::this_thread::sleep(boost::posix_time::seconds(TIMEOUT_BETWEEN));
	}
}
#endif

#ifdef UDP_ENABLED
void udp_poll() {
	while (FOREVER)
		udp_client->poll();
}

void udp_func(void *id) {
	int i = 1;
	while (FOREVER) {
		std::cout << "UDP Client " << id << ": run " << i++
				  << " sending " << ITERATIONS << " messages."
				  << std::endl;
		send_udp_messages(udp_consumer, udp_test_message, true);
		boost::this_thread::sleep(boost::posix_time::seconds(TIMEOUT_BETWEEN));
	}
}
#endif

void print_count() {
	while (FOREVER) {
		uint32_t m, b;
#ifdef TCP_ENABLED
		m = tcp_consumer->get_statistics()->get_received_messages_count();
		b = tcp_consumer->get_statistics()->get_received_bytes_count();
		std::cout << "Received " << m << " TCP messages (" << b << " bytes)." << std::endl;
		std::cout << tcp_receiver.receive_counter << std::endl;
#endif
#ifdef UDP_ENABLED
		m = udp_consumer->get_statistics()->get_received_messages_count();
		b = udp_consumer->get_statistics()->get_received_bytes_count();
		std::cout << "Received " << m << " UDP messages (" << b << " bytes)." << std::endl;
		std::cout << udp_receiver.receive_counter << std::endl;
#endif
		boost::this_thread::sleep(boost::posix_time::seconds(5));
	}
}


int main(int argc, char **argv) {

	vsomeip::factory * default_factory = vsomeip::factory::get_default_factory();
#ifdef TCP_ENABLED
	vsomeip::endpoint * tcp_target
		= default_factory->get_endpoint("127.0.0.1", VSOMEIP_LOWEST_VALID_PORT,
										vsomeip::ip_protocol::TCP, vsomeip::ip_version::V4);
	tcp_client = default_factory->create_client();
	tcp_consumer = tcp_client->create_consumer(tcp_target);
	tcp_consumer->register_for(&tcp_receiver, 0x3333, 0x4444);
	tcp_consumer->start();
#endif

#ifdef UDP_ENABLED
	vsomeip::endpoint * udp_target
		= default_factory->get_endpoint("127.0.0.1", VSOMEIP_LOWEST_VALID_PORT,
										vsomeip::ip_protocol::UDP, vsomeip::ip_version::V4);
	udp_client = default_factory->create_client();
	udp_consumer = udp_client->create_consumer(udp_target);
	udp_consumer->register_for(&udp_receiver, 0x3333, 0x4444);
	udp_consumer->start();
#endif
#ifdef TCP_ENABLED
	tcp_test_message = default_factory->create_message();
	tcp_test_message->set_method_id(0x2222);
	vsomeip::payload &tcp_test_payload = tcp_test_message->get_payload();
	uint8_t tcp_test_data[MESSAGE_PAYLOAD_SIZE];
	for (int i = 0; i < MESSAGE_PAYLOAD_SIZE; ++i) tcp_test_data[i] = (i % 256);
	tcp_test_payload.set_data(tcp_test_data, sizeof(tcp_test_data));
#endif
#ifdef UDP_ENABLED
	udp_test_message = default_factory->create_message();
	udp_test_message->set_service_id(0x1111);
	vsomeip::payload &udp_test_payload = udp_test_message->get_payload();
	uint8_t udp_test_data[MESSAGE_PAYLOAD_SIZE];
	for (int i = 0; i < MESSAGE_PAYLOAD_SIZE; ++i) udp_test_data[i] = 0xFF;
	udp_test_payload.set_data(udp_test_data, sizeof(udp_test_data));
#endif
#ifdef TCP_ENABLED
	const vsomeip::statistics *tcp_statistics = tcp_consumer->get_statistics();
#endif
#ifdef UDP_ENABLED
	const vsomeip::statistics *udp_statistics = udp_consumer->get_statistics();
#endif

	boost::thread print_thread(print_count);

#ifdef TCP_ENABLED
	boost::thread tcp_thread_0(tcp_func, (void*)0);
	boost::thread tcp_thread_1(tcp_func, (void*)1);
	boost::thread tcp_thread_2(tcp_func, (void*)2);
	boost::thread tcp_thread_3(tcp_func, (void*)3);
	boost::thread tcp_thread_4(tcp_func, (void*)4);
	boost::thread tcp_thread_5(tcp_func, (void*)5);
	boost::thread tcp_thread_6(tcp_func, (void*)6);
	boost::thread tcp_thread_7(tcp_func, (void*)7);
	boost::thread tcp_thread_8(tcp_func, (void*)8);
	boost::thread tcp_thread_9(tcp_func, (void*)9);

	boost::thread tcp_poll_thread(tcp_poll);
#endif
#ifdef UDP_ENABLED
	boost::thread udp_thread_0(udp_func, (void*)0);
	boost::thread udp_thread_1(udp_func, (void*)1);
	boost::thread udp_thread_2(udp_func, (void*)2);
	boost::thread udp_thread_3(udp_func, (void*)0);
	boost::thread udp_thread_4(udp_func, (void*)4);
	boost::thread udp_thread_5(udp_func, (void*)5);
	boost::thread udp_thread_6(udp_func, (void*)6);
	boost::thread udp_thread_7(udp_func, (void*)7);
	boost::thread udp_thread_8(udp_func, (void*)8);
	boost::thread udp_thread_9(udp_func, (void*)9);

	boost::thread udp_poll_thread(udp_poll);
#endif
#ifdef TCP_ENABLED
	tcp_thread_0.join();
	tcp_thread_1.join();
	tcp_thread_2.join();
	tcp_thread_3.join();
	tcp_thread_4.join();
	tcp_thread_5.join();
	tcp_thread_6.join();
	tcp_thread_7.join();
	tcp_thread_8.join();
	tcp_thread_9.join();

	tcp_poll_thread.join();
#endif
#ifdef UDP_ENABLED
	udp_thread_0.join();
	udp_thread_1.join();
	udp_thread_2.join();
	udp_thread_3.join();
	udp_thread_4.join();
	udp_thread_5.join();
	udp_thread_6.join();
	udp_thread_7.join();
	udp_thread_8.join();
	udp_thread_9.join();

	udp_poll_thread.join();
#endif

#ifdef TCP_ENABLED
	std::cout << "Sent " << tcp_statistics->get_sent_messages_count() << " TCP messages, containing "
						 << tcp_statistics->get_sent_bytes_count() << " bytes."
						 << std::endl;
#endif
#ifdef UDP_ENABLED
	std::cout << "Sent " << udp_statistics->get_sent_messages_count() << " UDP messages, containing "
						 << udp_statistics->get_sent_bytes_count() << " bytes."
						 << std::endl;
#endif

	return 0;
}
