// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <thread>

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>
#include <vsomeip/internal/logger.hpp>

#include "unknown_sd_option_type_test_globals.hpp"

class unknown_sd_option_type_service {
public:
    unknown_sd_option_type_service(struct unknown_sd_option_type_test::service_info _service_info) :
            service_info_(_service_info),
            app_(vsomeip::runtime::get()->create_application("unknown_sd_option_type_service")),
            wait_until_registered_(true),
            wait_until_shutdown_method_called_(true),
            subscription_accepted_asynchronous_(false),
            subscription_accepted_synchronous_(false),
            offer_thread_(std::bind(&unknown_sd_option_type_service::run, this)) {
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return;
        }
        app_->register_state_handler(
                std::bind(&unknown_sd_option_type_service::on_state, this,
                        std::placeholders::_1));

        // offer field
        std::set<vsomeip::eventgroup_t> its_eventgroups;
        its_eventgroups.insert(_service_info.eventgroup_id);
        app_->offer_event(service_info_.service_id, 0x1,
                    service_info_.event_id,
                    its_eventgroups, vsomeip::event_type_e::ET_FIELD,
                    std::chrono::milliseconds::zero(),
                    false, true, nullptr, vsomeip::reliability_type_e::RT_UNRELIABLE);

        its_eventgroups.clear();
        its_eventgroups.insert(static_cast<vsomeip::eventgroup_t>(_service_info.eventgroup_id + 1u));

        app_->offer_event(service_info_.service_id, 0x1,
                static_cast<vsomeip::event_t>(service_info_.event_id + 1u),
                its_eventgroups, vsomeip::event_type_e::ET_FIELD,
                std::chrono::milliseconds::zero(),
                false, true, nullptr, vsomeip::reliability_type_e::RT_UNRELIABLE);

        app_->register_message_handler(vsomeip::ANY_SERVICE,
                vsomeip::ANY_INSTANCE, service_info_.shutdown_method_id,
                std::bind(&unknown_sd_option_type_service::on_shutdown_method_called, this,
                        std::placeholders::_1));

        app_->register_async_subscription_handler(service_info_.service_id,
                0x1, service_info_.eventgroup_id,
                std::bind(&unknown_sd_option_type_service::subscription_handler_async,
                          this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                          std::placeholders::_4, std::placeholders::_5));

        app_->start();
    }

    ~unknown_sd_option_type_service() {
        offer_thread_.join();
    }

    void offer() {
        app_->offer_service(service_info_.service_id, 0x1);
    }

    void stop() {
        app_->stop_offer_service(service_info_.service_id, 0x1);
        app_->clear_all_handler();
        app_->stop();
    }

    void on_state(vsomeip::state_type_e _state) {
        VSOMEIP_INFO << "Application " << app_->get_name() << " is "
        << (_state == vsomeip::state_type_e::ST_REGISTERED ?
                "registered." : "deregistered.");

        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            std::lock_guard<std::mutex> its_lock(mutex_);
            wait_until_registered_ = false;
            condition_.notify_one();
        }
    }

    void on_shutdown_method_called(const std::shared_ptr<vsomeip::message> &_message) {
        app_->send(vsomeip::runtime::get()->create_response(_message));
        VSOMEIP_WARNING << "************************************************************";
        VSOMEIP_WARNING << "Shutdown method called -> going down!";
        VSOMEIP_WARNING << "************************************************************";
        std::lock_guard<std::mutex> its_lock(mutex_);
        wait_until_shutdown_method_called_ = false;
        condition_.notify_one();
    }

    void run() {
        VSOMEIP_DEBUG << '[' << std::setw(4) << std::setfill('0') << std::hex
                << service_info_.service_id << "] Running";
        std::unique_lock<std::mutex> its_lock(mutex_);
        while (wait_until_registered_) {
            condition_.wait(its_lock);
        }

        VSOMEIP_DEBUG << '[' << std::setw(4) << std::setfill('0') << std::hex
                << service_info_.service_id << "] Offering";
        offer();

        while (!subscription_accepted_asynchronous_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        async_subscription_handler_(true);

        while (wait_until_shutdown_method_called_) {
            condition_.wait(its_lock);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        stop();
    }

    void subscription_handler_async(vsomeip::client_t _client, std::uint32_t, std::uint32_t,
                                    bool _subscribed, const std::function<void(const bool)>& _cbk) {
        VSOMEIP_WARNING << __func__ << ' ' << std::hex << _client << " subscribed." << _subscribed;

        async_subscription_handler_ = _cbk;
        static int was_called = 0;
        was_called++;
        EXPECT_EQ(1, was_called);
        EXPECT_TRUE(_subscribed);
        subscription_accepted_asynchronous_ = true;

    }

private:
    struct unknown_sd_option_type_test::service_info service_info_;
    std::shared_ptr<vsomeip::application> app_;

    bool wait_until_registered_;
    bool wait_until_shutdown_method_called_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> subscription_accepted_asynchronous_;
    std::atomic<bool> subscription_accepted_synchronous_;
    std::thread offer_thread_;
    std::function<void(const bool)> async_subscription_handler_;
};

TEST(someip_unknown_sd_option_type_service, send_subscription) {
    unknown_sd_option_type_service its_sample(unknown_sd_option_type_test::service);
}


#if defined(__linux__)
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
#endif
