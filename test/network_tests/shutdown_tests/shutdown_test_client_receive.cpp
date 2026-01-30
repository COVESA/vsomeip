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
#include <atomic>
#include <iomanip>

#include "shutdown_test_globals.hpp"
#include <vsomeip/internal/logger.hpp>
#include <common/vsomeip_app_utilities.hpp>

#include "common/timeout_detector.hpp"

class shutdown_test_client {
public:
    shutdown_test_client();

    void run_test() {
        std::unique_lock<std::mutex> its_lock(mutex_);
        ASSERT_TRUE(condition_.wait_for(its_lock, std::chrono::seconds(10), [this] { return is_available_; }))
                << "Service not available in time";

        request_->set_service(shutdown_test::TEST_SERVICE_SERVICE_ID);
        request_->set_instance(shutdown_test::TEST_SERVICE_INSTANCE_ID);
        request_->set_method(shutdown_test::STOP_METHOD);
        request_->set_reliable(shutdown_test_client::is_tcp_);
        request_->set_payload(0);

        app_->send(request_);

        ASSERT_TRUE(condition_.wait_for(its_lock, std::chrono::seconds(10), [this] { return to_stop; }))
                << "Did not receive the shutdown reply in time";

        stop();
    }

    bool init() {
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return false;
        }

        app_->register_message_handler(shutdown_test::TEST_SERVICE_SERVICE_ID, shutdown_test::TEST_SERVICE_INSTANCE_ID,
                                       shutdown_test::STOP_METHOD, std::bind(&shutdown_test_client::on_shutdown_method_reply, this));

        app_->register_availability_handler(shutdown_test::TEST_SERVICE_SERVICE_ID, shutdown_test::TEST_SERVICE_INSTANCE_ID,
                                            std::bind(&shutdown_test_client::on_availability, this, std::placeholders::_1,
                                                      std::placeholders::_2, std::placeholders::_3));

        app_->request_service(shutdown_test::TEST_SERVICE_SERVICE_ID, shutdown_test::TEST_SERVICE_INSTANCE_ID, false);
        return true;
    }

    void start() {
        VSOMEIP_INFO << "Starting Client...";
        app_->start();
    }

    void stop() {
        VSOMEIP_INFO << "Stopping Client...";
        app_->stop();
    }

    void on_shutdown_method_reply() {
        n_messages_received_++;
        VSOMEIP_INFO << "Shutdown message " << n_messages_received_ << " received.";
        if (n_messages_received_ == shutdown_test::SHUTDOWN_NUMBER_MESSAGES) {
            VSOMEIP_INFO << "All shutdown messages received, preparing to stop.";
            std::lock_guard<std::mutex> its_lock(mutex_);
            to_stop = true;
            condition_.notify_one();
        }
    }

    void join_sender_thread() { sender_.join(); }

    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
        VSOMEIP_INFO << "Service [" << std::hex << std::setfill('0') << std::setw(4) << _service << "." << _instance << "] is "
                     << (_is_available ? "available." : "NOT available.");

        std::lock_guard<std::mutex> its_lock(mutex_);
        if (is_available_ && !_is_available) {
            is_available_ = false;
        } else if (_is_available && !is_available_) {
            is_available_ = true;
            condition_.notify_one();
        }
    }

    static bool is_tcp_;

private:
    std::shared_ptr<vsomeip::application> app_;
    std::shared_ptr<vsomeip::message> request_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool is_available_;
    bool to_stop;
    std::thread sender_;
    int n_messages_received_{0};
};

shutdown_test_client::shutdown_test_client() :
    app_(vsomeip::runtime::get()->create_application("shutdown_test_client")), request_(vsomeip::runtime::get()->create_request(true)),
    is_available_(false), to_stop(false), sender_(std::bind(&shutdown_test_client::run_test, this)) { }

TEST(someip_shutdown_test, send_messages_to_service_and_wait_for_shutdown_reply) {
    shutdown_test_client test_client_;
    if (test_client_.init()) {
        test_client_.start();
        test_client_.join_sender_thread();
    }
}

bool shutdown_test_client::is_tcp_ = false;

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    timeout_detector td;
    if (argc > 1) {
        if (std::string("TCP") == std::string(argv[1])) {
            shutdown_test_client::is_tcp_ = true;
        } else {
            shutdown_test_client::is_tcp_ = false;
        }
    }
    ::testing::GTEST_FLAG(throw_on_failure) = true;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
