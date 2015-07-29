// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <memory>
#include <thread>

#include <vsomeip/vsomeip.hpp>

#include "../examples/sample-ids.hpp"
#include "../implementation/runtime/include/application_impl.hpp"
#include "../implementation/routing/include/routing_manager.hpp"

class client_sample {
public:
	client_sample()
		: app_(new vsomeip::application_impl("")),
		  runner_(std::bind(&client_sample::run, this)),
		  is_available_(false),
		  is_blocked_(false) {
	}

	void init() {
		VSOMEIP_INFO << "Initializing...";
		app_->init();

		app_->register_event_handler(
				std::bind(
					&client_sample::on_event,
					this,
					std::placeholders::_1));

		app_->register_availability_handler(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID,
				std::bind(&client_sample::on_availability,
						  this,
						  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

		app_->register_message_handler(
				vsomeip::ANY_SERVICE, SAMPLE_INSTANCE_ID, vsomeip::ANY_METHOD,
				std::bind(&client_sample::on_message,
						  this,
						  std::placeholders::_1));
	}

	void start() {
		VSOMEIP_INFO << "Starting...";
		app_->start();
	}

	void stop() {
		VSOMEIP_INFO << "Stopping...";
		app_->stop();
	}

	void on_event(vsomeip::event_type_e _event) {
		if (_event == vsomeip::event_type_e::ET_REGISTERED) {
			VSOMEIP_INFO << "Client registration done.";
			app_->request_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, false,
								  vsomeip::ANY_MAJOR, vsomeip::ANY_MINOR,
								  vsomeip::ANY_TTL);
		}
	}

	void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
		VSOMEIP_INFO << "Service ["
				<< std::setw(4) << std::setfill('0') << std::hex << _service << "." << _instance
				<< "] is "
				<< (_is_available ? "available." : "NOT available.");

		if (SAMPLE_SERVICE_ID == _service && SAMPLE_INSTANCE_ID == _instance) {
			static bool is_available = false;
			if (is_available  && !_is_available) is_available = false;
			else if (_is_available && !is_available) {
				is_available = true;
				std::lock_guard< std::mutex > its_lock(mutex_);
				is_blocked_ = true;
				condition_.notify_one();
			}
		}
	}

	void on_message(const std::shared_ptr< vsomeip::message > &_response) {
		VSOMEIP_INFO << "Received a response from Service ["
				<< std::setw(4) << std::setfill('0') << std::hex << _response->get_service()
				<< "."
				<< std::setw(4) << std::setfill('0') << std::hex << _response->get_instance()
				<< "] to Client/Session ["
				<< std::setw(4) << std::setfill('0') << std::hex << _response->get_client()
				<< "/"
				<< std::setw(4) << std::setfill('0') << std::hex << _response->get_session()
				<< "]";
	}

	void join() {
		runner_.join();
	}

	void run() {
		std::unique_lock< std::mutex > its_lock(mutex_);
		while (!is_blocked_) condition_.wait(its_lock);
		VSOMEIP_INFO << "Running...";

		vsomeip::routing_manager *its_routing = app_->get_routing_manager();

		vsomeip::byte_t its_good_payload_data[] = {
				0x12, 0x34, 0x84, 0x21,
		    	0x00, 0x00, 0x00, 0x11,
		    	0x13, 0x43, 0x00, 0x00,
		    	0x01, 0x00, 0x00, 0x00,
		    	0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01
		};

		vsomeip::byte_t its_bad_payload_data[] = {
				0x12, 0x34, 0x84, 0x21,
				0x00, 0x00, 0x01, 0x23,
				0x13, 0x43, 0x00, 0x00,
				0x01, 0x00, 0x00, 0x00,
				0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01
		};

		// Test sequence
		its_good_payload_data[11] = 0x01;
		its_routing->send(0x1343, its_good_payload_data, sizeof(its_good_payload_data), SAMPLE_INSTANCE_ID, true, true);
		its_bad_payload_data[11] = 0x02;
		its_routing->send(0x1343, its_bad_payload_data, sizeof(its_bad_payload_data), SAMPLE_INSTANCE_ID, true, true);
		its_good_payload_data[11] = 0x03;
		its_routing->send(0x1343, its_good_payload_data, sizeof(its_good_payload_data), SAMPLE_INSTANCE_ID, true, true);
		its_bad_payload_data[11] = 0x04;
		its_routing->send(0x1343, its_bad_payload_data, sizeof(its_bad_payload_data), SAMPLE_INSTANCE_ID, true, true);
		its_bad_payload_data[11] = 0x05;
		its_routing->send(0x1343, its_bad_payload_data, sizeof(its_bad_payload_data), SAMPLE_INSTANCE_ID, true, true);
		its_good_payload_data[11] = 0x06;
		its_routing->send(0x1343, its_good_payload_data, sizeof(its_good_payload_data), SAMPLE_INSTANCE_ID, true, true);
		its_good_payload_data[11] = 0x07;
		its_routing->send(0x1343, its_good_payload_data, sizeof(its_good_payload_data), SAMPLE_INSTANCE_ID, true, true);
		its_bad_payload_data[11] = 0x08;
		its_routing->send(0x1343, its_bad_payload_data, sizeof(its_bad_payload_data), SAMPLE_INSTANCE_ID, true, true);
		its_bad_payload_data[11] = 0x09;
		its_routing->send(0x1343, its_bad_payload_data, sizeof(its_bad_payload_data), SAMPLE_INSTANCE_ID, true, true);
		its_bad_payload_data[11] = 0x0A;
		its_routing->send(0x1343, its_bad_payload_data, sizeof(its_bad_payload_data), SAMPLE_INSTANCE_ID, true, true);
		its_good_payload_data[11] = 0x0B;
		its_routing->send(0x1343, its_good_payload_data, sizeof(its_good_payload_data), SAMPLE_INSTANCE_ID, true, true);
		its_good_payload_data[11] = 0x0C;
		its_routing->send(0x1343, its_good_payload_data, sizeof(its_good_payload_data), SAMPLE_INSTANCE_ID, true, true);
		its_good_payload_data[11] = 0x0D;
		its_routing->send(0x1343, its_good_payload_data, sizeof(its_good_payload_data), SAMPLE_INSTANCE_ID, true, true);
		its_bad_payload_data[11] = 0x0E;
		its_routing->send(0x1343, its_bad_payload_data, sizeof(its_bad_payload_data), SAMPLE_INSTANCE_ID, true, true);
		its_good_payload_data[11] = 0x0F;
		its_routing->send(0x1343, its_good_payload_data, sizeof(its_good_payload_data), SAMPLE_INSTANCE_ID, true, true);

		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		stop();
	}

private:
	std::shared_ptr< vsomeip::application_impl > app_;
	std::thread runner_;
	std::mutex mutex_;
	std::condition_variable condition_;
	bool is_available_;
	bool is_blocked_;
};


int main(int argc, char **argv) {
	client_sample its_client;
	its_client.init();
	its_client.start();
	its_client.join();
	return 0;
}


