// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <thread>
#include <vsomeip/internal/logger.hpp>
#include <vsomeip/vsomeip.hpp>

#ifdef ANDROID
#include "../../configuration/include/internal_android.hpp"
#else
#include "../../configuration/include/internal.hpp"
#endif // ANDROID

#include "any_instance_test_globals.hpp"

enum operation_mode_e { SUBSCRIBE,
    METHODCALL };

class any_instance_test_client {
public:
    any_instance_test_client(
        std::array<any_instance_test::service_info, NUMBER_SERVICES>
            _service_infos,
        vsomeip::service_t _service_id,
        struct any_instance_test::service_info _client,
        bool _use_tcp)
        : service_infos_(_service_infos)
        , service_id_(_service_id)
        , client_(_client)
        , app_(vsomeip::runtime::get()->create_application(
              "any_instance_client"))
        , use_tcp_(_use_tcp)
        , wait_until_registered_(true)
        , wait_for_stop_(true)
        , send_thread_(std::bind(&any_instance_test_client::send, this))
        , offer_thread_(std::bind(&any_instance_test_client::run, this))
    {
        for (int i = 0; i < NUMBER_SERVICES; i++) {
            service_available_[i] = false;
        }
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return;
        }
        app_->register_state_handler(std::bind(&any_instance_test_client::on_state,
            this, std::placeholders::_1));

        app_->register_message_handler(
            vsomeip::ANY_SERVICE, vsomeip::ANY_INSTANCE, vsomeip::ANY_METHOD,
            std::bind(&any_instance_test_client::on_message, this,
                std::placeholders::_1));

        // register availability for any instance of _service_id
        app_->register_availability_handler(
            _service_id, vsomeip::ANY_INSTANCE,
            std::bind(&any_instance_test_client::on_availability, this,
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3));
        app_->request_service(_service_id, vsomeip::ANY_INSTANCE);

        app_->start();
    }

    ~any_instance_test_client()
    {
        send_thread_.join();
        offer_thread_.join();
    }

    void on_state(vsomeip::state_type_e _state)
    {
        VSOMEIP_INFO << "Application " << app_->get_name() << " is "
                     << (_state == vsomeip::state_type_e::ST_REGISTERED
                                ? "registered."
                                : "deregistered.");

        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            std::lock_guard<std::mutex> its_lock(mutex_);
            wait_until_registered_ = false;
            condition_.notify_one();
        }
    }

    void on_availability(vsomeip::service_t _service,
        vsomeip::instance_t _instance, bool _is_available)
    {
        VSOMEIP_INFO << "Service [" << std::setw(4) << std::setfill('0') << std::hex
                     << _service << "." << _instance << "] is "
                     << (_is_available ? "available" : "not available") << ".";
        std::lock_guard<std::mutex> its_lock(mutex_);
        for (int i = 0; i < NUMBER_SERVICES; i++) {
            if (_instance == service_infos_[i].instance_id) {
                service_available_[i] = _is_available;
            }
        }
        condition_.notify_one();
    }

    void on_message(const std::shared_ptr<vsomeip::message>& _message)
    {
        if (_message->get_message_type() == vsomeip::message_type_e::MT_RESPONSE) {
            on_response(_message);
        }
    }

    void on_response(const std::shared_ptr<vsomeip::message>& _message)
    {
        std::unique_lock<std::mutex> its_lock(mutex_);
        EXPECT_EQ(service_infos_[current_service_].service_id,
            _message->get_service());
        EXPECT_EQ(service_infos_[current_service_].method_id,
            _message->get_method());
        EXPECT_EQ(service_infos_[current_service_].instance_id,
            _message->get_instance());
        if (service_infos_[current_service_].service_id == _message->get_service() && service_infos_[current_service_].instance_id == _message->get_instance() && service_infos_[current_service_].instance_id == _message->get_instance()) {
            response_received_ = true;
        }
        condition_.notify_one();
    }

    void offer() { app_->offer_service(client_.service_id, client_.instance_id); }

    void run()
    {
        VSOMEIP_DEBUG << "[" << std::setw(4) << std::setfill('0') << std::hex
                      << client_.service_id << "] Running";
        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (wait_until_registered_) {
                condition_.wait(its_lock);
            }

            VSOMEIP_DEBUG << "[" << std::setw(4) << std::setfill('0') << std::hex
                          << client_.service_id << "] Offering";
            offer();
            condition_.notify_one();
        }

        VSOMEIP_DEBUG << "Running";
        while (wait_for_stop_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        VSOMEIP_DEBUG << "Stop running";
    }

    void send()
    {
        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (wait_until_registered_) {
                condition_.wait(its_lock);
            }

            for (current_service_ = 0; current_service_ < NUMBER_SERVICES;
                 current_service_++) {
                VSOMEIP_INFO << "Waiting for availability of service "
                             << current_service_;
                while (service_available_[current_service_] == false) {
                    condition_.wait(its_lock);
                }
                std::shared_ptr<vsomeip::message> its_req = vsomeip::runtime::get()->create_request();
                its_req->set_service(service_infos_[current_service_].service_id);
                its_req->set_instance(service_infos_[current_service_].instance_id);
                its_req->set_method(service_infos_[current_service_].method_id);
                its_req->set_reliable(use_tcp_);
                response_received_ = false;
                app_->send(its_req);
                VSOMEIP_INFO << "Waiting for response of service " << current_service_;
                while (response_received_ == false) {
                    condition_.wait(its_lock);
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        VSOMEIP_INFO << "going down.";
        wait_for_stop_ = false;
        app_->stop_offer_service(client_.service_id, client_.instance_id);
        app_->clear_all_handler();
        app_->stop();
    }

private:
    std::array<any_instance_test::service_info, NUMBER_SERVICES> service_infos_;
    vsomeip::service_t service_id_;
    struct any_instance_test::service_info client_;
    operation_mode_e operation_mode_;
    std::shared_ptr<vsomeip::application> app_;
    bool use_tcp_;

    bool wait_until_registered_;
    bool service_available_[NUMBER_SERVICES];
    bool response_received_;
    std::uint32_t current_service_;
    std::mutex mutex_;
    std::condition_variable condition_;

    bool wait_for_stop_;
    std::mutex stop_mutex_;
    std::condition_variable stop_condition_;

    // std::thread stop_thread_;
    std::thread send_thread_;
    std::thread offer_thread_;
};

static bool use_tcp;

TEST(someip_any_instance_test, subscribe_or_call_method_at_service)
{
    any_instance_test_client its_sample(any_instance_test::service_infos,
        any_instance_test::service_id,
        any_instance_test::client,
        use_tcp);
}

#ifndef _WIN32
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    if(argc < 2) {
        std::cerr << "Please specify a method type of the service, like: " << argv[0] << " UDP" << std::endl;
        std::cerr << "Valid method types include:" << std::endl;
        std::cerr << "[UDP, TCP]" << std::endl;
        return 1;
    }

    if(std::string("TCP") == std::string(argv[1])) {
        use_tcp = true;
    } else if(std::string("UDP") == std::string(argv[1])) {
        use_tcp = false;
    } else {
        std::cerr << "Wrong method call type passed, exiting" << std::endl;
        std::cerr << "Valid method call types include:" << std::endl;
        std::cerr << "[UDP, TCP]" << std::endl;
        return 1;
    }

    return RUN_ALL_TESTS();
}
#endif
