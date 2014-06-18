#include <chrono>
#include <iostream>

#include <boost/bind/placeholders.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include <vsomeip/vsomeip.hpp>

//#define SAMPLE_MULTITHREAD
#define SAMPLE_MESSAGE_SIZE 60

#define INTERNAL_SAMPLE_SERVICE			 	0x1234
#define INTERNAL_SAMPLE_SERVICE_INSTANCE	0x5678
#define INTERNAL_SAMPLE_METHOD			 	0x0205
#define INTERNAL_SAMPLE_EVENTGROUP			0x4263
#define INTERNAL_SAMPLE_EVENT				0x0077

#define EXTERNAL_SAMPLE_SERVICE			 	0x1234
#define EXTERNAL_SAMPLE_SERVICE_INSTANCE	0x2356
#define EXTERNAL_SAMPLE_METHOD			 	0x0203
#define EXTERNAL_SAMPLE_EVENTGROUP			0x4815
#define EXTERNAL_SAMPLE_EVENT				0x0077

using namespace vsomeip;

factory * the_factory = factory::get_instance();
application * the_application = the_factory->create_application("InternalClientSample");

message * the_message = the_factory->create_message();

class Connection {
public:
	void on_method(std::shared_ptr< message > &_message) {
		static int i = 0;

		const payload &its_payload = _message->get_payload();
		uint32_t length = its_payload.get_length();
		const uint8_t *data = its_payload.get_data();

		std::cout << "[" << std::dec << std::setw(4) << std::setfill('0') << i++
				  << "] Service has answered to client "
				  << std::hex << _message->get_client_id()
				  << " with [ ";
		for (uint32_t j = 0; j < length; ++j)
			std::cout << std::setw(2) << std::setfill('0') << std::hex << (int)data[j] << " ";
		std::cout << "]" << std::endl;
	}

	void on_event(std::shared_ptr< message > &_message) {
		static int i = 0;

		const payload &its_payload = _message->get_payload();
		uint32_t length = its_payload.get_length();
		const uint8_t *data = its_payload.get_data();

		std::cout << "[" << std::dec << std::setw(4) << std::setfill('0') << i++
				  << "] Service has updated field ["
				  << std::hex << _message->get_method_id()
				  << "] for client ["
				  << std::hex << _message->get_client_id()
				  << "] to [ ";
		for (uint32_t j = 0; j < length; ++j)
			std::cout << std::setw(2) << std::setfill('0') << std::hex << (int)data[j] << " ";
		std::cout << "]" << std::endl;
	}

	void on_avail(service_id s, instance_id i, bool a) {
		std::cout << "Service [" << std::hex << s << "." << i << "] is "
				  << (a ? "available!" : "not available!") << std::endl;
	}
};

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
	the_application->start();
}


int main(int argc, char **argv) {
	the_application->init(argc, argv);
	Connection the_connection;
	std::function< void (service_id, instance_id, bool) > avail_handler
			= std::bind(&Connection::on_avail, &the_connection, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	the_application->register_availability_handler(INTERNAL_SAMPLE_SERVICE, INTERNAL_SAMPLE_SERVICE_INSTANCE, avail_handler);

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

	the_application->request_eventgroup(INTERNAL_SAMPLE_SERVICE, INTERNAL_SAMPLE_SERVICE_INSTANCE, INTERNAL_SAMPLE_EVENTGROUP);
	the_application->request_eventgroup(EXTERNAL_SAMPLE_SERVICE, EXTERNAL_SAMPLE_SERVICE_INSTANCE, EXTERNAL_SAMPLE_EVENTGROUP);

	boost::function< void (std::shared_ptr< message > &) > method_handler = boost::bind(&Connection::on_method, &the_connection, _1);
	boost::function< void (std::shared_ptr< message > &) > event_handler = boost::bind(&Connection::on_event, &the_connection, _1);

	the_application->register_message_handler(INTERNAL_SAMPLE_SERVICE, INTERNAL_SAMPLE_SERVICE_INSTANCE, INTERNAL_SAMPLE_METHOD, method_handler);
	the_application->register_message_handler(EXTERNAL_SAMPLE_SERVICE, EXTERNAL_SAMPLE_SERVICE_INSTANCE, EXTERNAL_SAMPLE_METHOD, method_handler);

	the_application->register_message_handler(INTERNAL_SAMPLE_SERVICE, INTERNAL_SAMPLE_SERVICE_INSTANCE, INTERNAL_SAMPLE_EVENT, event_handler);

	boost::thread framework_thread(run);
	boost::thread application_thread(worker);

	framework_thread.join();
	application_thread.join();

	return 0;
}




