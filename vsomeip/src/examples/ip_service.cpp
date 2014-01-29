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
#include <boost/thread.hpp>
#include <vsomeip/vsomeip.hpp>

#define TCP_ENABLED 1
#define UDP_ENABLED 1

#ifdef TCP_ENABLED
vsomeip::service *tcp_service = 0;
#endif

vsomeip::service *udp_service = 0;

class mymessagereceiver : public vsomeip::receiver {
public:
	mymessagereceiver() : receive_counter(0), service_(0), id_(last_id__++) {};

	void set_service(vsomeip::service *_service) { service_ = _service; };

	void receive(const vsomeip::message_base *_message) {
		vsomeip::endpoint *from = _message->get_endpoint();
		if (from) {
			std::cout << "Client " << id_ << " received a "
					  << (from->get_protocol() == vsomeip::ip_protocol::UDP ? "UDP" : "TCP")
					  << (int)from->get_version()
					  << " message from "
					  << from->get_address()
					  << ":"
					  << from->get_port()
					  << std::endl;

#if 1
			// sending back
			vsomeip::message *answer = vsomeip::factory::get_default_factory()->create_message();
			answer->set_service_id(0x3333);
			answer->set_method_id(0x4444);
			answer->set_endpoint(_message->get_endpoint());
			vsomeip::payload& answer_payload = answer->get_payload();
			uint8_t payload_data[] = { 0x1, 0x2, 0x3, 0x4, 0x5 };
			answer_payload.set_data(payload_data, sizeof(payload_data));
			service_->send(answer);
#endif
		}
		receive_counter++;
	}

	int receive_counter;
	vsomeip::service *service_;

	int id_;
	static int last_id__;
};

int mymessagereceiver::last_id__ = 0;

#ifdef TCP_ENABLED
mymessagereceiver tr0;
mymessagereceiver tr1;
mymessagereceiver tr2;
mymessagereceiver tr3;
#endif

mymessagereceiver ur0;
mymessagereceiver ur1;
mymessagereceiver ur2;
mymessagereceiver ur3;

void print_count() {
	while (1) {
		uint32_t m, b;
#ifdef TCP_ENABLED
		m = tcp_service->get_statistics()->get_received_messages_count();
		b = tcp_service->get_statistics()->get_received_bytes_count();
		std::cout << "Received " << m << " TCP messages (" << b << " bytes)." << std::endl;
		std::cout << tr0.receive_counter << " "
				  << tr1.receive_counter << " "
				  << tr2.receive_counter << " "
				  << tr3.receive_counter << std::endl;
#endif

		m = udp_service->get_statistics()->get_received_messages_count();
		b = udp_service->get_statistics()->get_received_bytes_count();
		std::cout << "Received " << m << " UDP messages (" << b << " bytes)." << std::endl;
		std::cout << ur0.receive_counter << " "
				  << ur1.receive_counter << " "
				  << ur2.receive_counter << " "
				  << ur3.receive_counter << std::endl;

		boost::this_thread::sleep(boost::posix_time::seconds(5));
	}
}

void udp_receive() {
	while (1) {
		udp_service->poll();
	};
}

#ifdef TCP_ENABLED
void tcp_receive() {
	while (1) { tcp_service->poll_one(); };
}
#endif

int main(int argc, char **argv) {
	vsomeip::factory *default_factory = vsomeip::factory::get_default_factory();
#ifdef TCP_ENABLED
	vsomeip::endpoint *tcp_target
		= default_factory->create_endpoint("127.0.0.1", VSOMEIP_FIRST_VALID_PORT,
				vsomeip::ip_protocol::TCP, vsomeip::ip_version::V4);

	tcp_service = vsomeip::factory::get_default_factory()->create_service(tcp_target);

	tr0.set_service(tcp_service);
	tr1.set_service(tcp_service);
	tr2.set_service(tcp_service);
	tr3.set_service(tcp_service);

	tcp_service->register_for(&tr0, 0x1111, 0x2222);
	tcp_service->register_for(&tr1, 0x1112, 0x2222);
	tcp_service->register_for(&tr2, 0x1111, 0x2222);
	tcp_service->register_for(&tr3, 0x1112, 0x2222);
	tcp_service->start();

#endif

	vsomeip::endpoint *udp_target
			= default_factory->create_endpoint("127.0.0.1", VSOMEIP_FIRST_VALID_PORT,
					vsomeip::ip_protocol::UDP, vsomeip::ip_version::V4);
	udp_service = vsomeip::factory::get_default_factory()->create_service(udp_target);

	ur0.set_service(udp_service);
	ur1.set_service(udp_service);
	ur2.set_service(udp_service);
	ur3.set_service(udp_service);

	udp_service->register_for(&ur0, 0x1111, 0x1222);
	udp_service->register_for(&ur1, 0x1111, 0x2222);
	udp_service->register_for(&ur2, 0x1111, 0x1222);
	udp_service->register_for(&ur3, 0x1111, 0x2222);
	udp_service->start();

	boost::thread print_thread(print_count);

#ifdef TCP_ENABLED
	boost::thread tcp_receiver(tcp_receive);
#endif
	boost::thread udp_receiver(udp_receive);

#ifdef TCP_ENABLED
	tcp_receiver.join();
#endif
	udp_receiver.join();

	return 0;
}



