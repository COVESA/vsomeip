#include <chrono>
#include <iostream>

#include <boost/thread.hpp>

#include <vsomeip/vsomeip.hpp>

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

factory * the_factory = factory::get_instance();
application * the_application = the_factory->create_application("ExternalClientSample");

message * the_message = the_factory->create_message();

void receive(const message_base *_message) {
	static int i = 0;

	std::cout << "[" << std::dec << std::setw(4) << std::setfill('0') << i++
			  << "] Service has answered to Client "
			  << std::hex << _message->get_client_id()
			  << " with "
			  << std::dec << _message->get_length() << " bytes."
			  << std::endl;
}

void worker() {
	bool is_sending_to_internal = true;
	while (1) {
		bool is_available;

		if (is_sending_to_internal) {
			the_message->set_service_id(INTERNAL_SAMPLE_SERVICE);
			the_message->set_instance_id(INTERNAL_SAMPLE_SERVICE_INSTANCE);
			the_message->set_method_id(INTERNAL_SAMPLE_METHOD);
			is_available = the_application->is_service_available(INTERNAL_SAMPLE_SERVICE, INTERNAL_SAMPLE_SERVICE_INSTANCE);
		} else {
			the_message->set_service_id(EXTERNAL_SAMPLE_SERVICE);
			the_message->set_instance_id(EXTERNAL_SAMPLE_SERVICE_INSTANCE);
			the_message->set_method_id(EXTERNAL_SAMPLE_METHOD);
			is_available = the_application->is_service_available(EXTERNAL_SAMPLE_SERVICE, EXTERNAL_SAMPLE_SERVICE_INSTANCE);
		}

		usleep(100000);

		if (is_available)
			the_application->send(the_message, true);

		is_sending_to_internal = !is_sending_to_internal;
	}
}

void run() {
	the_application->init(options_count, options);

	// Client-Id will be automatically set by the application
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

	the_application->register_message_handler(INTERNAL_SAMPLE_SERVICE, INTERNAL_SAMPLE_SERVICE_INSTANCE, INTERNAL_SAMPLE_METHOD, receive);
	the_application->register_message_handler(EXTERNAL_SAMPLE_SERVICE, EXTERNAL_SAMPLE_SERVICE_INSTANCE, EXTERNAL_SAMPLE_METHOD, receive);

	the_application->start();
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
