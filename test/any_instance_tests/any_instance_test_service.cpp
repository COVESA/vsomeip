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

#include "any_instance_test_globals.hpp"

static unsigned long service_number;

class any_instance_test_service {
public:
    any_instance_test_service(
        struct any_instance_test::service_info _service_info)
        : service_info_(_service_info)
        , app_(vsomeip::runtime::get()->create_application())
        , counter_(0)
        , wait_until_registered_(true)
        , shutdown_method_called_(false)
        , offer_thread_(std::bind(&any_instance_test_service::run, this))
    {
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return;
        }
        app_->register_state_handler(std::bind(&any_instance_test_service::on_state,
            this, std::placeholders::_1));

        app_->register_message_handler(
            service_info_.service_id, service_info_.instance_id,
            service_info_.method_id,
            std::bind(&any_instance_test_service::on_request, this,
                std::placeholders::_1));

        app_->start();
    }

    ~any_instance_test_service() { offer_thread_.join(); }

    void offer()
    {
        app_->offer_service(service_info_.service_id, service_info_.instance_id);
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

    void on_request(const std::shared_ptr<vsomeip::message>& _message)
    {
        app_->send(vsomeip::runtime::get()->create_response(_message));
        std::this_thread::sleep_for(std::chrono::seconds(1));
        VSOMEIP_WARNING
            << "************************************************************";
        VSOMEIP_WARNING << "Shutdown method called -> going down!";
        VSOMEIP_WARNING
            << "************************************************************";
        shutdown_method_called_ = true;
        app_->stop_offer_service(service_info_.service_id,
            service_info_.instance_id);
        app_->clear_all_handler();
        app_->stop();
    }

    void
    on_shutdown_method_called(const std::shared_ptr<vsomeip::message>& _message)
    {
        (void)_message;
    }

    void run()
    {
        VSOMEIP_DEBUG << "[" << std::setw(4) << std::setfill('0') << std::hex
                      << service_info_.service_id << "] Running";
        std::unique_lock<std::mutex> its_lock(mutex_);
        while (wait_until_registered_) {
            condition_.wait(its_lock);
        }

        VSOMEIP_DEBUG << "[" << std::setw(4) << std::setfill('0') << std::hex
                      << service_info_.service_id << "] Offering";
        offer();

        while (!shutdown_method_called_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

private:
    struct any_instance_test::service_info service_info_;
    std::shared_ptr<vsomeip::application> app_;
    std::uint32_t counter_;

    bool wait_until_registered_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> shutdown_method_called_;
    std::thread offer_thread_;
};

TEST(someip_any_instance_test, offer_any_service)
{
    any_instance_test_service its_sample(
        any_instance_test::service_infos[service_number]);
}

#ifndef _WIN32
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    if (argc < 2) {
        std::cerr << "Please specify a service number, like: " << argv[0] << " 2"
                  << std::endl;
        return 1;
    }
    service_number = std::stoul(std::string(argv[1]), nullptr);
    if (service_number > NUMBER_SERVICES) {
        std::cerr << "Only services 0-" << NUMBER_SERVICES - 1 << " are configured"
                  << std::endl;
    }
    return RUN_ALL_TESTS();
}
#endif
