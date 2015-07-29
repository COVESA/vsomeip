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

class service_sample {
public:
    service_sample(bool _use_static_routing) :
            app_(vsomeip::runtime::get()->create_application()),
            is_registered_(false),
            use_static_routing_(_use_static_routing),
            offer_thread_(std::bind(&service_sample::run, this)) {
    }

    void init() {
        std::lock_guard<std::mutex> its_lock(mutex_);

        app_->init();
        app_->register_message_handler(
        SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, SAMPLE_METHOD_ID,
                std::bind(&service_sample::on_message, this,
                        std::placeholders::_1));

        app_->register_event_handler(
                std::bind(&service_sample::on_event, this,
                        std::placeholders::_1));

        VSOMEIP_INFO<< "Static routing " << (use_static_routing_ ? "ON" : "OFF");
    }

    void start() {
        app_->start();
    }

    void offer() {
        app_->offer_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
        app_->offer_service(SAMPLE_SERVICE_ID + 1, SAMPLE_INSTANCE_ID);
    }

    void stop_offer() {
        app_->stop_offer_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
        app_->stop_offer_service(SAMPLE_SERVICE_ID + 1, SAMPLE_INSTANCE_ID);
    }

    void on_event(vsomeip::event_type_e _event) {
        VSOMEIP_INFO << "Application " << app_->get_name() << " is "
        << (_event == vsomeip::event_type_e::ET_REGISTERED ?
                "registered." : "deregistered.");

        if (_event == vsomeip::event_type_e::ET_REGISTERED) {
            if (!is_registered_) {
                is_registered_ = true;
                blocked_ = true;
                condition_.notify_one();
            }
        } else {
            is_registered_ = false;
        }
    }

    void on_message(const std::shared_ptr<vsomeip::message> &_request) {
        VSOMEIP_INFO << "Received a message with Client/Session [" << std::setw(4)
        << std::setfill('0') << std::hex << _request->get_client() << "/"
        << std::setw(4) << std::setfill('0') << std::hex
        << _request->get_session() << "]";

        std::shared_ptr<vsomeip::message> its_response = vsomeip::runtime::get()
        ->create_response(_request);

        std::shared_ptr<vsomeip::payload> its_payload = vsomeip::runtime::get()
        ->create_payload();
        std::vector<vsomeip::byte_t> its_payload_data;
        for (std::size_t i = 0; i < 120; ++i)
        its_payload_data.push_back(i % 256);
        its_payload->set_data(its_payload_data);
        its_response->set_payload(its_payload);

        app_->send(its_response, true);
    }

    void run() {
        std::unique_lock<std::mutex> its_lock(mutex_);
        while (!blocked_)
            condition_.wait(its_lock);

        bool is_offer(true);

        if (use_static_routing_) {
            offer();
            while (true);
        } else {
            while (true) {
                if (is_offer)
                    offer();
                else
                    stop_offer();
                std::this_thread::sleep_for(std::chrono::milliseconds(10000));
                is_offer = !is_offer;
            }
        }
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    bool is_registered_;
    bool use_static_routing_;

    std::thread offer_thread_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool blocked_;
};

int main(int argc, char **argv) {
    bool use_static_routing(false);

    std::string static_routing_enable("--static-routing");

    for (int i = 1; i < argc; i++) {
        if (static_routing_enable == argv[i]) {
            use_static_routing = true;
        }
    }

    service_sample its_sample(use_static_routing);
    its_sample.init();
    its_sample.start();

    return 0;
}
