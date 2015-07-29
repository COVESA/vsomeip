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
    service_sample(bool _use_tcp, uint32_t _cycle) :
            app_(vsomeip::runtime::get()->create_application()), is_registered_(
                    false), use_tcp_(_use_tcp), cycle_(_cycle), offer_thread_(
                    std::bind(&service_sample::run, this)), notify_thread_(
                    std::bind(&service_sample::notify, this)), is_offered_(
                    false) {
    }

    void init() {
        std::lock_guard<std::mutex> its_lock(mutex_);

        app_->init();
        app_->register_event_handler(
                std::bind(&service_sample::on_event, this,
                        std::placeholders::_1));

        app_->register_message_handler(
                SAMPLE_SERVICE_ID,
                SAMPLE_INSTANCE_ID,
                SAMPLE_GET_METHOD_ID,
                std::bind(&service_sample::on_get, this,
                          std::placeholders::_1));

        app_->register_message_handler(
                SAMPLE_SERVICE_ID,
                SAMPLE_INSTANCE_ID,
                SAMPLE_SET_METHOD_ID,
                std::bind(&service_sample::on_set, this,
                          std::placeholders::_1));

        payload_ = vsomeip::runtime::get()->create_payload();

        blocked_ = true;
        condition_.notify_one();
    }

    void start() {
        app_->start();
    }

    void offer() {
        std::lock_guard<std::mutex> its_lock(notify_mutex_);
        app_->offer_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
        is_offered_ = true;
        notify_condition_.notify_one();
    }

    void stop_offer() {
        app_->stop_offer_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
        is_offered_ = false;
    }

    void on_event(vsomeip::event_type_e _event) {
        VSOMEIP_INFO << "Application " << app_->get_name() << " is "
        << (_event == vsomeip::event_type_e::ET_REGISTERED ?
                "registered." : "deregistered.");

        if (_event == vsomeip::event_type_e::ET_REGISTERED) {
            if (!is_registered_) {
                is_registered_ = true;
            }
        } else {
            is_registered_ = false;
        }
    }

    void on_get(const std::shared_ptr<vsomeip::message> &_message) {
        std::shared_ptr<vsomeip::message> its_response
            = vsomeip::runtime::get()->create_response(_message);
        its_response->set_payload(payload_);
        app_->send(its_response, true);
    }

    void on_set(const std::shared_ptr<vsomeip::message> &_message) {
        payload_ = _message->get_payload();

        std::shared_ptr<vsomeip::message> its_response
            = vsomeip::runtime::get()->create_response(_message);
        its_response->set_payload(payload_);
        app_->send(its_response, true);
        app_->notify(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID,
                     SAMPLE_EVENT_ID, payload_);
    }

    void run() {
        std::unique_lock<std::mutex> its_lock(mutex_);
        while (!blocked_)
            condition_.wait(its_lock);

        bool is_offer(true);
        while (true) {
            if (is_offer)
                offer();
            else
                stop_offer();

            std::this_thread::sleep_for(std::chrono::milliseconds(10000));
            is_offer = !is_offer;
        }
    }

    void notify() {
        std::shared_ptr<vsomeip::message> its_message
            = vsomeip::runtime::get()->create_request(use_tcp_);

        its_message->set_service(SAMPLE_SERVICE_ID);
        its_message->set_instance(SAMPLE_INSTANCE_ID);
        its_message->set_method(SAMPLE_SET_METHOD_ID);

        vsomeip::byte_t its_data[10];
        uint32_t its_size = 1;

        while (true) {
            std::unique_lock<std::mutex> its_lock(notify_mutex_);
            while (!is_offered_)
                notify_condition_.wait(its_lock);
            while (is_offered_) {
                if (its_size == sizeof(its_data))
                    its_size = 1;

                for (uint32_t i = 0; i < its_size; ++i)
                    its_data[i] = static_cast<uint8_t>(i);

                payload_->set_data(its_data, its_size);

                VSOMEIP_INFO << "Setting event (Length=" << std::dec << its_size << ").";
                app_->notify(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, SAMPLE_EVENT_ID, payload_);

                its_size++;

                std::this_thread::sleep_for(std::chrono::milliseconds(cycle_));
            }
        }
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    bool is_registered_;
    bool use_tcp_;
    uint32_t cycle_;

    std::thread offer_thread_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool blocked_;

    std::thread notify_thread_;
    std::mutex notify_mutex_;
    std::condition_variable notify_condition_;
    bool is_offered_;

    std::shared_ptr<vsomeip::payload> payload_;
};

int main(int argc, char **argv) {
    bool use_tcp = false;
    uint32_t cycle = 1000; // default 1s

    std::string tcp_enable("--tcp");
    std::string udp_enable("--udp");
    std::string cycle_arg("--cycle");

    for (int i = 1; i < argc; i++) {
        if (tcp_enable == argv[i]) {
            use_tcp = true;
            break;
        }
        if (udp_enable == argv[i]) {
            use_tcp = false;
            break;
        }

        if (cycle_arg == argv[i] && i + 1 < argc) {
            i++;
            std::stringstream converter;
            converter << argv[i];
            converter >> cycle;
        }
    }

    service_sample its_sample(use_tcp, cycle);
    its_sample.init();
    its_sample.start();

    return 0;
}
