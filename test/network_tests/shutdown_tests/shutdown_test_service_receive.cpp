// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <queue>
#include <iomanip>

#include "shutdown_test_globals.hpp"
#include <vsomeip/internal/logger.hpp>
#include "common/timeout_detector.hpp"

class shutdown_test_service {
public:
    shutdown_test_service();

    void run_test() {
        std::unique_lock<std::mutex> its_lock(mutex_);
        ASSERT_TRUE(condition_.wait_for(its_lock, std::chrono::seconds(10), [&] { return is_registered_; }))
                << "Service not registered in time";

        app_->offer_service(shutdown_test::TEST_SERVICE_SERVICE_ID, shutdown_test::TEST_SERVICE_INSTANCE_ID);

        ASSERT_TRUE(condition_.wait_for(its_lock, std::chrono::seconds(10), [&] { return to_stop; }))
                << "Did not received the Shutdown message in time";

        stop();
    }

    bool init() {
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return false;
        }
        app_->register_message_handler(shutdown_test::TEST_SERVICE_SERVICE_ID, shutdown_test::TEST_SERVICE_INSTANCE_ID,
                                       shutdown_test::STOP_METHOD, std::bind(&shutdown_test_service::on_stop, this));
        app_->register_state_handler(std::bind(&shutdown_test_service::on_state, this, std::placeholders::_1));
        return true;
    }

    void start() {
        VSOMEIP_INFO << "Starting Service...";
        app_->start();
    }

    void stop() {
        VSOMEIP_INFO << "Stopping Service...";
        app_->stop_offer_service(shutdown_test::TEST_SERVICE_SERVICE_ID, shutdown_test::TEST_SERVICE_INSTANCE_ID);
        app_->stop();
    }

    void on_stop() {
        n_messages_received_++;
        VSOMEIP_INFO << "Shutdown message " << n_messages_received_ << " received.";
        if (n_messages_received_ == shutdown_test::SHUTDOWN_NUMBER_MESSAGES) {
            VSOMEIP_INFO << "All shutdown messages received, preparing to stop.";
            std::unique_lock<std::mutex> lk(mutex_);
            to_stop = true;
            condition_.notify_one();
        }
    }

    void join_offer_thread() { offer_thread_.join(); }

    void on_state(vsomeip::state_type_e _state) {
        VSOMEIP_INFO << "Application " << app_->get_name() << " is "
                     << (_state == vsomeip::state_type_e::ST_REGISTERED ? "registered." : "deregistered.");

        std::lock_guard<std::mutex> its_lock(mutex_);
        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            if (!is_registered_) {
                is_registered_ = true;
                condition_.notify_one();
            }
        } else {
            is_registered_ = false;
        }
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    bool is_registered_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool to_stop;
    std::thread offer_thread_;
    int n_messages_received_{0};
};

shutdown_test_service::shutdown_test_service() :
    app_(vsomeip::runtime::get()->create_application("shutdown_test_service")), is_registered_(false), to_stop(false),
    offer_thread_(std::bind(&shutdown_test_service::run_test, this)) { }

TEST(someip_shutdown_test, receive_message__send_reply_and_wait_for_shutdown) {
    shutdown_test_service test_service;
    if (test_service.init()) {
        test_service.start();
        test_service.join_offer_thread();
    }
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    timeout_detector td;
    ::testing::GTEST_FLAG(throw_on_failure) = true;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
