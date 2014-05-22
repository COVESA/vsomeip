#include <iomanip>
#include <iostream>

#include <boost/thread.hpp>

#include <vsomeip/vsomeip.hpp>

using namespace vsomeip;

#define EXTERNAL_SAMPLE_SERVICE			 	0x1234
#define EXTERNAL_SAMPLE_SERVICE_INSTANCE	0x2356
#define EXTERNAL_SAMPLE_METHOD			 	0x0203
#define EXTERNAL_SAMPLE_EVENTGROUP			0x4815

factory *the_factory = 0;
application *the_application = 0;

void on_off() {
	static bool is_on = false;

	while (true) {
		usleep(3000000);

		if (is_on) {
			std::cout << "Stopping service" << std::endl;
			the_application->stop_service(EXTERNAL_SAMPLE_SERVICE, EXTERNAL_SAMPLE_SERVICE_INSTANCE);
		} else {
			std::cout << "Starting service" << std::endl;
			the_application->start_service(EXTERNAL_SAMPLE_SERVICE, EXTERNAL_SAMPLE_SERVICE_INSTANCE);
		}

		is_on = !is_on;
	}
}

void receive_message(const message *_message) {
	static int i = 0;

	std::cout << "[" << std::dec << std::setw(4) << std::setfill('0') << i++
			  << "] Client "
			  << std::hex << _message->get_client_id()
			  << " has sent a request with "
			  << std::dec << _message->get_length() << " bytes."
			  << std::endl;

	message *response = the_factory->create_response(_message);

	uint8_t payload_data[] = { 0x11, 0x22, 0x44, 0x66, 0x88 };
	response->get_payload().set_data(payload_data, sizeof(payload_data));

	the_application->send(response);
}

int main(int argc, char **argv) {
	the_factory = factory::get_instance();
	endpoint *location = the_factory->get_endpoint("127.0.0.1", 30498, ip_protocol::TCP);

	// create the application and provide a service at the defined location
	the_application = the_factory->create_application("ExternalServiceSample");
	the_application->init(argc, argv);

	the_application->provide_service(EXTERNAL_SAMPLE_SERVICE, EXTERNAL_SAMPLE_SERVICE_INSTANCE, location);

	the_application->provide_eventgroup(EXTERNAL_SAMPLE_SERVICE, EXTERNAL_SAMPLE_SERVICE_INSTANCE, EXTERNAL_SAMPLE_EVENTGROUP, 0);

	the_application->register_message_handler(EXTERNAL_SAMPLE_SERVICE, EXTERNAL_SAMPLE_SERVICE_INSTANCE, EXTERNAL_SAMPLE_METHOD, receive_message);

	boost::thread on_off_thread(on_off);

	the_application->start();
}
