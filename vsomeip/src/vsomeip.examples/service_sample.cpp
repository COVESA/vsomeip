#include <chrono>

#include <boost/thread.hpp>

#include <vsomeip/vsomeip.hpp>

using namespace vsomeip;

#define INTERNAL_SAMPLE_SERVICE			 	0x1234
#define INTERNAL_SAMPLE_SERVICE_INSTANCE	0x5678
#define INTERNAL_SAMPLE_METHOD			 	0x0205
#define INTERNAL_SAMPLE_EVENTGROUP			0x4263
#define INTERNAL_SAMPLE_FIELD				0x0077

int options_count = 0;
char **options = 0;

factory *the_factory = 0;
application *the_application = 0;
field *the_field;

void on_off() {
	static bool is_on = false;
	int i = 0;

	payload &its_payload = the_field->get_payload();
	uint8_t data[2];

	while (true) {
		usleep(1000000);

		if (i < 9) {
			if (is_on) {
				data[0] = i;
				data[1] = 10-i;
				std::cout << "Updating field ["
						  << std::hex << INTERNAL_SAMPLE_FIELD
						  << "] to ["
						  << std::setw(2) << std::setfill('0') << (int)data[0] << " "
						  << std::setw(2) << std::setfill('0') << (int)data[1] << "]"
						  << std::endl;
				its_payload.set_data(data, sizeof(data));
			}
		} else {
			if (is_on) {
				std::cout << "Stopping service" << std::endl;
				the_application->stop_service(INTERNAL_SAMPLE_SERVICE, INTERNAL_SAMPLE_SERVICE_INSTANCE);
			} else {
				std::cout << "Starting service" << std::endl;
				the_application->start_service(INTERNAL_SAMPLE_SERVICE, INTERNAL_SAMPLE_SERVICE_INSTANCE);
			}

			is_on = !is_on;
		}

		i ++;
		if (i == 10) i = 0;
	}
}

void receive_message(std::shared_ptr< message > &_message) {
	static int i = 0;

	std::cout << "[" << std::dec << std::setw(4) << std::setfill('0') << i++
			  << "] Client "
			  << std::hex << _message->get_client_id()
			  << " has sent a request with "
			  << std::dec << _message->get_length() << " bytes."
			  << std::endl;

	message *response = the_factory->create_response(_message.get());

	uint8_t payload_data[] = { 0x11, 0x22, 0x44, 0x66, 0x88 };
	response->get_payload().set_data(payload_data, sizeof(payload_data));

	the_application->send(response);
}

int main(int argc, char **argv) {
	the_factory = factory::get_instance();
	endpoint *location = the_factory->get_endpoint("127.0.0.1", 30498, ip_protocol::UDP);
	endpoint *multicast = the_factory->get_endpoint("224.0.0.1", 31000, ip_protocol::UDP);

	// create the application and provide a service at the defined location
	the_application = the_factory->create_application("InternalServiceSample");
	the_application->init(argc, argv);

	the_application->provide_service(INTERNAL_SAMPLE_SERVICE, INTERNAL_SAMPLE_SERVICE_INSTANCE, location);
	the_application->provide_eventgroup(INTERNAL_SAMPLE_SERVICE, INTERNAL_SAMPLE_SERVICE_INSTANCE, INTERNAL_SAMPLE_EVENTGROUP, multicast);

	the_field = the_factory->create_field(the_application, INTERNAL_SAMPLE_SERVICE, INTERNAL_SAMPLE_SERVICE_INSTANCE, INTERNAL_SAMPLE_FIELD);

	uint8_t data[] = { 0x77, 0x88, 0x99 };
	the_field->get_payload().set_data(data, sizeof(data));

	the_application->add_field(INTERNAL_SAMPLE_SERVICE, INTERNAL_SAMPLE_SERVICE_INSTANCE, INTERNAL_SAMPLE_EVENTGROUP, the_field);

	the_application->register_message_handler(INTERNAL_SAMPLE_SERVICE, INTERNAL_SAMPLE_SERVICE_INSTANCE, INTERNAL_SAMPLE_METHOD, receive_message);

	boost::thread on_off_thread(on_off);

	the_application->start();
}




