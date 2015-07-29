// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#include <vsomeip/vsomeip.hpp>

#include "sample-ids.hpp"

class client_sample {
public:
	client_sample(bool _use_tcp) :
			app_(vsomeip::runtime::get()->create_application()), use_tcp_(
					_use_tcp) {
	}

	void init() {
		app_->init();

		VSOMEIP_INFO << "Client settings [protocol="
				<< (use_tcp_ ? "TCP" : "UDP")
				<< "]";

		app_->register_availability_handler(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID,
				std::bind(&client_sample::on_availability,
						  this,
						  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

		app_->register_message_handler(vsomeip::ANY_SERVICE, SAMPLE_INSTANCE_ID,
				vsomeip::ANY_METHOD,
				std::bind(&client_sample::on_message, this,
						std::placeholders::_1));
	}

	void start() {
		app_->start();
	}

	void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
		VSOMEIP_INFO << "Service ["
				<< std::setw(4) << std::setfill('0') << std::hex << _service << "." << _instance
				<< "] is "
				<< (_is_available ? "available." : "NOT available.");

		if (_is_available && SAMPLE_SERVICE_ID == _service && SAMPLE_INSTANCE_ID == _instance) {
			app_->subscribe(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, SAMPLE_EVENTGROUP_ID);
		}
	}

	void on_message(const std::shared_ptr<vsomeip::message> &_response) {
		std::stringstream its_message;
		its_message << "Received a notification for Event [" << std::setw(4)
				<< std::setfill('0') << std::hex << _response->get_service()
				<< "." << _response->get_instance() << "."
				<< _response->get_method() << "] to Client/Session ["
				<< std::setw(4) << std::setfill('0') << std::hex
				<< _response->get_client() << "/" << std::setw(4)
				<< std::setfill('0') << std::hex << _response->get_session()
				<< "] = ";
		std::shared_ptr<vsomeip::payload> its_payload =
				_response->get_payload();
		its_message << "(" << std::dec << its_payload->get_length() << ") ";
		for (uint32_t i = 0; i < its_payload->get_length(); ++i)
			its_message << std::hex << std::setw(2) << std::setfill('0')
				<< (int) its_payload->get_data()[i] << " ";
		VSOMEIP_INFO << its_message.str();

		if (_response->get_client() == 0) {
			if ((its_payload->get_length() % 5) == 0) {
				std::shared_ptr<vsomeip::message> its_get
					= vsomeip::runtime::get()->create_request();
				its_get->set_service(SAMPLE_SERVICE_ID);
				its_get->set_instance(SAMPLE_INSTANCE_ID);
				its_get->set_method(SAMPLE_GET_METHOD_ID);
				its_get->set_reliable(use_tcp_);
				app_->send(its_get, true);
			}

			if ((its_payload->get_length() % 8) == 0) {
				std::shared_ptr<vsomeip::message> its_set
					= vsomeip::runtime::get()->create_request();
				its_set->set_service(SAMPLE_SERVICE_ID);
				its_set->set_instance(SAMPLE_INSTANCE_ID);
				its_set->set_method(SAMPLE_SET_METHOD_ID);
				its_set->set_reliable(use_tcp_);

				const vsomeip::byte_t its_data[]
				    = { 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
				    	0x48, 0x49, 0x50, 0x51, 0x52 };
				std::shared_ptr<vsomeip::payload> its_set_payload
					= vsomeip::runtime::get()->create_payload();
				its_set_payload->set_data(its_data, sizeof(its_data));
				its_set->set_payload(its_set_payload);
				app_->send(its_set, true);
			}
		}
	}

private:
	std::shared_ptr< vsomeip::application > app_;
	bool use_tcp_;
	bool be_quiet_;
};

int main(int argc, char **argv) {
	bool use_tcp = false;
	bool be_quiet = false;
	uint32_t cycle = 1000;  // Default: 1s

	std::string tcp_enable("--tcp");
	std::string udp_enable("--udp");

	int i = 1;
	while (i < argc) {
		if (tcp_enable == argv[i]) {
			use_tcp = true;
		} else if (udp_enable == argv[i]) {
			use_tcp = false;
		}
		i++;
	}

	client_sample its_sample(use_tcp);
	its_sample.init();
	its_sample.start();
	return 0;
}
