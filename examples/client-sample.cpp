// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
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
	client_sample(bool _use_tcp, bool _be_quiet, uint32_t _cycle)
		: app_(vsomeip::runtime::get()->create_application()),
		  request_(vsomeip::runtime::get()->create_request()),
		  use_tcp_(_use_tcp),
		  be_quiet_(_be_quiet),
		  cycle_(_cycle),
		  sender_(std::bind(&client_sample::run, this)),
		  running_(true) {
	}

	void init() {
		app_->init();

		VSOMEIP_INFO << "Client settings [protocol="
				<< (use_tcp_ ? "TCP" : "UDP")
				<< ":quiet="
				<< (be_quiet_ ? "true" : "false")
				<< ":cycle="
				<< cycle_
				<< "]";

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

	void on_event(vsomeip::event_type_e _event) {
		if (_event == vsomeip::event_type_e::REGISTERED) {
			//app_->offer_service(OTHER_SAMPLE_SERVICE_ID, OTHER_SAMPLE_INSTANCE_ID);
			app_->request_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
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
				send();
			}
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
		if (!be_quiet_)
		{
			std::lock_guard< std::mutex > its_lock(mutex_);
			blocked_ = true;
			condition_.notify_one();
		}
	}

	void run() {
		while (running_) {
			{
				std::unique_lock< std::mutex > its_lock(mutex_);
				while (!blocked_) condition_.wait(its_lock);
				std::this_thread::sleep_for(std::chrono::milliseconds(cycle_));
				app_->send(request_, true, use_tcp_);
				VSOMEIP_INFO << "Client/Session ["
						<< std::setw(4) << std::setfill('0') << std::hex << request_->get_client()
						<< "/"
						<< std::setw(4) << std::setfill('0') << std::hex << request_->get_session()
						<< "] sent a request to Service ["
						<< std::setw(4) << std::setfill('0') << std::hex << request_->get_service()
						<< "."
						<< std::setw(4) << std::setfill('0') << std::hex << request_->get_instance()
						<< "]";
				blocked_ = false;
			}
		}
	}

private:
	std::shared_ptr< vsomeip::application > app_;
	std::shared_ptr< vsomeip::message > request_;
	bool use_tcp_;
	bool be_quiet_;
	uint32_t cycle_;
	vsomeip::session_t session_;
	std::mutex mutex_;
	std::condition_variable condition_;
	std::thread sender_;
	bool running_;
	bool blocked_;
};


int main(int argc, char **argv) {
	bool use_tcp = false;
	bool be_quiet = false;
	uint32_t cycle = 1000; // Default: 1s

	std::string tcp_enable("--tcp");
	std::string udp_enable("--udp");
	std::string quiet_enable("--quiet");
	std::string cycle_arg("--cycle");

	int i = 1;
	while (i < argc) {
		if (tcp_enable == argv[i]) {
			use_tcp = true;
		} else if (udp_enable == argv[i]) {
			use_tcp = false;
		} else if (quiet_enable == argv[i]) {
			be_quiet = true;
		} else if (cycle_arg == argv[i] && i+1 < argc) {
			i++;
			std::stringstream converter;
			converter << argv[i];
			converter >> cycle;
		}
		i++;
	}

	client_sample its_sample(use_tcp, be_quiet, cycle);
	its_sample.init();
	its_sample.start();
	return 0;
}
