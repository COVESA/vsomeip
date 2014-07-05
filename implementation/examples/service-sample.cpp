// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <iostream>
#include <thread>

#include <vsomeip/vsomeip.hpp>

#include "sample-ids.hpp"

class service_sample {
public:
	service_sample()
		: app_(vsomeip::runtime::get()->create_application()), is_registered_(false) {
	}

	void init() {
		app_->register_message_handler(
					SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, SAMPLE_METHOD_ID,
					std::bind(&service_sample::on_message,
					this,
					std::placeholders::_1)
				);

		app_->register_event_handler(
				std::bind(&service_sample::on_event, this, std::placeholders::_1));

		app_->init();
	}

	void start() {
		app_->start();
	}

	void offer() {
		app_->offer_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
	}

	void on_event(vsomeip::event_type_e _event) {
		VSOMEIP_INFO << "Application " << app_->get_name()
					 << " is "
				     << (_event == vsomeip::event_type_e::REGISTERED ? "registered." : "deregistered.");

		if (_event == vsomeip::event_type_e::REGISTERED) {
			if (!is_registered_) {
				is_registered_= true;
				offer();
			}
		} else {
			is_registered_ = false;
		}
	}

	void on_message(std::shared_ptr< vsomeip::message > &_request) {
		VSOMEIP_INFO << "Received a message with Client/Session ["
				  << std::setw(4) << std::setfill('0') << std::hex << _request->get_client()
				  << "/"
				  << std::setw(4) << std::setfill('0') << std::hex << _request->get_session()
				  << "]";

		std::shared_ptr< vsomeip::message > its_response
			= vsomeip::runtime::get()->create_response(_request);

		std::shared_ptr< vsomeip::payload > its_payload = vsomeip::runtime::get()->create_payload();
		std::vector< vsomeip::byte_t > its_payload_data;
		for (std::size_t i = 0; i < 6; ++i) its_payload_data.push_back(i % 256);
		its_payload->set_data(its_payload_data);
		its_response->set_payload(its_payload);

		app_->send(its_response);
	}

private:
	std::shared_ptr< vsomeip::application > app_;
	bool is_registered_;
};


int main(int argc, char **argv) {
	service_sample its_sample;
	its_sample.init();
	its_sample.start();

	return 0;
}
