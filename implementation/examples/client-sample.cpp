// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

#include <vsomeip/vsomeip.hpp>

#include "sample-ids.hpp"

class client_sample {
public:
	client_sample(const std::string &_name)
		: app_(vsomeip::runtime::get()->create_application(_name)),
		  name_(_name), request_(vsomeip::runtime::get()->create_request()) {
	}

	void init(int argc, char **argv) {
		app_->init();
		app_->register_availability_handler(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID,
				std::bind(&client_sample::on_availability,
						  this,
						  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

		app_->offer_service(OTHER_SAMPLE_SERVICE_ID, OTHER_SAMPLE_INSTANCE_ID);
		app_->request_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
		app_->register_message_handler(
				vsomeip::VSOMEIP_ANY_SERVICE, SAMPLE_INSTANCE_ID, vsomeip::VSOMEIP_ANY_METHOD,
				std::bind(&client_sample::on_message,
						  this,
						  std::placeholders::_1));

		request_->set_service(SAMPLE_SERVICE_ID);
		request_->set_instance(SAMPLE_INSTANCE_ID);
		request_->set_method(SAMPLE_METHOD_ID);

		std::shared_ptr< vsomeip::payload > its_payload = vsomeip::runtime::get()->create_payload();
		std::vector< vsomeip::byte_t > its_payload_data;
		for (std::size_t i = 0; i < 10; ++i) its_payload_data.push_back(i % 256);
		its_payload->set_data(its_payload_data);
		request_->set_payload(its_payload);
	}

	void start() {
		app_->start();
	}

	void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
		VSOMEIP_INFO << "Service ["
				<< std::setw(4) << std::setfill('0') << std::hex << _service << "." << _instance
				<< "] is "
				<< (_is_available ? "available." : "NOT available.");

		if (SAMPLE_SERVICE_ID == _service && SAMPLE_INSTANCE_ID == _instance) {
			send();
		}
	}

	void on_message(std::shared_ptr< vsomeip::message > &_response) {
		VSOMEIP_INFO << "Received a response from Service ["
				<< std::setw(4) << std::setfill('0') << std::hex << _response->get_service()
				<< "."
				<< std::setw(4) << std::setfill('0') << std::hex << _response->get_instance()
				<< "] to Client/Session ["
				<< std::setw(4) << std::setfill('0') << std::hex << _response->get_client()
				<< "/"
				<< std::setw(4) << std::setfill('0') << std::hex << _response->get_session()
				<< "]";

		send();
	}

	void send() {
		app_->send(request_);
		VSOMEIP_INFO << "Client/Session ["
				<< std::setw(4) << std::setfill('0') << std::hex << request_->get_client()
				<< "/"
				<< std::setw(4) << std::setfill('0') << std::hex << request_->get_session()
				<< "] sent a request to Service ["
				<< std::setw(4) << std::setfill('0') << std::hex << request_->get_service()
				<< "."
				<< std::setw(4) << std::setfill('0') << std::hex << request_->get_instance()
				<< "]";
	}

private:
	std::shared_ptr< vsomeip::application > app_;
	std::shared_ptr< vsomeip::message > request_;
	std::string name_;
};


int main(int argc, char **argv) {
	std::string its_sample_name("client-sample");

	int i = 0;
	while (i < argc-1) {
		if (std::string("--name") == argv[i]) {
			its_sample_name = argv[i+1];
			break;
		}

		i++;
	}

	client_sample its_sample(its_sample_name);
	its_sample.init(argc, argv);
	its_sample.start();
	return 0;
}
