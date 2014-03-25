#include <chrono>

#include <boost/thread.hpp>

#include <vsomeip/vsomeip.hpp>

using namespace vsomeip;

#define INTERNAL_SAMPLE_SERVICE			 	0x1234
#define INTERNAL_SAMPLE_SERVICE_INSTANCE	0x5678
#define INTERNAL_SAMPLE_METHOD			 	0x0205

int options_count = 0;
char **options = 0;

factory * the_factory = 0;
application * the_application = 0;
endpoint * the_endpoint = 0;

void receive_message(const message_base *_message) {
	static int i = 0;

	const endpoint *source = _message->get_source();
	const endpoint *target = _message->get_target();

	std::cout << i++
			  << ". Application " << _message->get_sender_id()
			  << " sends a request with "
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

	message *response = the_factory->create_response(_message);

	uint8_t payload_data[] = { 0x11, 0x22, 0x44, 0x66, 0x88 };
	response->get_payload().set_data(payload_data, sizeof(payload_data));

	the_application->send(response);
}

void run(const char *name) {
	the_factory = factory::get_instance();
	the_application = the_factory->create_application(name);
	the_endpoint = the_factory->get_endpoint("10.0.2.15", 30499, vsomeip::ip_protocol::TCP);

	the_application->init(options_count, options);
	the_application->start();

	the_application->provide_service(INTERNAL_SAMPLE_SERVICE, INTERNAL_SAMPLE_SERVICE_INSTANCE, the_endpoint);
	the_application->register_cbk(INTERNAL_SAMPLE_SERVICE, INTERNAL_SAMPLE_METHOD, receive_message);

	while (1) {
		the_application->run();
	}
}


int main(int argc, char **argv) {
	options_count = argc;
	options = argv;

	run("InternalServiceSample");

	return 0;
}




