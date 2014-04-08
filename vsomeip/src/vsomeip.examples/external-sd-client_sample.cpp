#include <chrono>
#include <iostream>

#include <boost/thread.hpp>

#include <vsomeip/sd/vsomeip.hpp>

//#define SAMPLE_MULTITHREAD
#define SAMPLE_MESSAGE_SIZE 30

#define INTERNAL_SAMPLE_SERVICE			 	0x1234
#define INTERNAL_SAMPLE_SERVICE_INSTANCE	0x5678
#define INTERNAL_SAMPLE_METHOD			 	0x0205

#define EXTERNAL_SAMPLE_SERVICE			 	0x1234
#define EXTERNAL_SAMPLE_SERVICE_INSTANCE	0x2356
#define EXTERNAL_SAMPLE_METHOD			 	0x0203

using namespace vsomeip;

int options_count = 0;
char **options = 0;

sd::factory * the_factory = sd::factory::get_instance();
application * the_application = the_factory->create_managing_application("ExternalSdClientSample");

message * the_message = the_factory->create_message();

void receive(const message_base *_message) {
	static int i = 0;

	const endpoint *source = _message->get_source();
	const endpoint *target = _message->get_target();

	std::cout << i++
			  << ". Response for "
			  << std::hex << _message->get_client_id()
			  << " with "
			  << _message->get_length() << " bytes (";
	if (source) {
		std::cout << source->get_address()
				  << ":" << std::dec << source->get_port();
	} else {
		std::cout << "UNKNOWN";
	}
	std::cout << " --> ";
	if (target) {
		std::cout << target->get_address()
				  << ":" << std::dec << target->get_port();
	} else {
		std::cout << "UNKNOWN";
	}
	std::cout << ")" << std::endl;
}

void worker() {
	bool is_sending_to_internal = true;
	while (1) {
		if (is_sending_to_internal) {
			the_message->set_service_id(INTERNAL_SAMPLE_SERVICE);
			the_message->set_instance_id(INTERNAL_SAMPLE_SERVICE_INSTANCE);
			the_message->set_service_id(INTERNAL_SAMPLE_SERVICE);
		} else {
			the_message->set_service_id(EXTERNAL_SAMPLE_SERVICE);
			the_message->set_instance_id(INTERNAL_SAMPLE_SERVICE_INSTANCE);
			the_message->set_method_id(EXTERNAL_SAMPLE_METHOD);
		}

		usleep(100000);
		the_application->send(the_message, true);

		is_sending_to_internal = !is_sending_to_internal;
	}
}

void run() {
	the_application->init(options_count, options);
	the_application->start();

	the_message->set_session_id(0x4234);
	the_message->set_message_type(message_type_enum::REQUEST);
	the_message->set_return_code(return_code_enum::OK);

	uint8_t sample_message_data[SAMPLE_MESSAGE_SIZE];
	for (int i = 0; i < SAMPLE_MESSAGE_SIZE; ++i) {
		sample_message_data[i] = (i % 256);
	}
	the_message->get_payload().set_data(sample_message_data, sizeof(sample_message_data));

	the_application->request_service(INTERNAL_SAMPLE_SERVICE, INTERNAL_SAMPLE_SERVICE_INSTANCE);
	the_application->request_service(EXTERNAL_SAMPLE_SERVICE, EXTERNAL_SAMPLE_SERVICE_INSTANCE);

	the_application->register_cbk(INTERNAL_SAMPLE_SERVICE, INTERNAL_SAMPLE_METHOD, receive);
	the_application->register_cbk(EXTERNAL_SAMPLE_SERVICE, EXTERNAL_SAMPLE_METHOD, receive);

	while (1) {
		the_application->run();
	}
}


int main(int argc, char **argv) {
	options_count = argc;
	options = argv;

	boost::thread receiver(run);
	boost::thread sender(worker);

	sender.join();
	receiver.join();

	return 0;
}
