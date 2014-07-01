// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <iostream>
#include <thread>

#include <vsomeip/vsomeip.hpp>

#define SAMPLE_SERVICE_ID	0x1234
#define SAMPLE_INSTANCE_ID	0x5678
#define SAMPLE_METHOD_ID	0x0421

class service_sample {
public:
	service_sample()
		: app_(vsomeip::runtime::get()->create_application("service-sample")) {
	}

	void init(int argc, char **argv) {
		app_->init(argc, argv);

		app_->register_message_handler(
			SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, SAMPLE_METHOD_ID,
			std::bind(&service_sample::on_message,
			this,
			std::placeholders::_1)
		);
	}

	void start() {
		app_->start();
	}

	void offer() {
		app_->offer_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
	}

	void on_message(std::shared_ptr< vsomeip::message > &_request) {
		VSOMEIP_INFO << "Received a message with Client/Session ["
				  << std::setw(4) << std::setfill('0') << std::hex << _request->get_client()
				  << "/"
				  << std::setw(4) << std::setfill('0') << std::hex << _request->get_session()
				  << "]";

		std::shared_ptr< vsomeip::message > its_response
			= vsomeip::runtime::get()->create_response(_request);

		vsomeip::payload &its_payload = its_response->get_payload();
		std::vector< vsomeip::byte_t > its_sample_payload;
		for (std::size_t i = 0; i < 6; ++i) its_sample_payload.push_back(i % 256);
		its_payload.set_data(its_sample_payload);

		app_->send(its_response);
	}

private:
	std::shared_ptr< vsomeip::application > app_;
};

void run(void *arg) {
	service_sample *its_sample = (service_sample*)arg;

	VSOMEIP_INFO << "Updating offer for service ["
			  << std::hex << SAMPLE_SERVICE_ID << "." << SAMPLE_INSTANCE_ID
			  << "]";
	its_sample->offer();
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}


int main(int argc, char **argv) {
	service_sample its_sample;
	its_sample.init(argc, argv);

	std::thread runner(run, &its_sample);

	its_sample.start();

	runner.join();

	return 0;
}
