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
#include "common/test_main.hpp"

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

        for (std::uint32_t i = 0; i < shutdown_test::SHUTDOWN_NUMBER_MESSAGES; ++i) {
            reply_->set_reliable(shutdown_test_service::is_tcp_);
            reply_->set_session(static_cast<vsomeip::session_t>(i + 1));
            std::shared_ptr<vsomeip::payload> its_payload = vsomeip::runtime::get()->create_payload();
            std::vector<vsomeip::byte_t> its_payload_data;
            its_payload_data.assign(shutdown_test_service::size_buffer_, shutdown_test::DATA_SERVICE_TO_CLIENT);
            its_payload->set_data(its_payload_data);
            reply_->set_payload(its_payload);

            app_->send(reply_);
        }

        VSOMEIP_INFO << "Shutdown reply sent";
        stop();
    }

    bool init() {
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return false;
        }
        app_->register_message_handler(shutdown_test::TEST_SERVICE_SERVICE_ID, shutdown_test::TEST_SERVICE_INSTANCE_ID,
                                       shutdown_test::STOP_METHOD,
                                       std::bind(&shutdown_test_service::on_stop_message, this, std::placeholders::_1));
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

    void on_stop_message(const std::shared_ptr<vsomeip::message>& _message) {
        VSOMEIP_INFO << "Shutdown method called, sending response";

        reply_ = vsomeip::runtime::get()->create_response(_message);

        std::lock_guard<std::mutex> its_lock(mutex_);
        to_stop = true;
        condition_.notify_one();
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

    static std::uint32_t size_buffer_;
    static bool is_tcp_;

private:
    std::shared_ptr<vsomeip::application> app_;
    std::shared_ptr<vsomeip::message> reply_;
    bool is_registered_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool to_stop;
    std::thread offer_thread_;
};

shutdown_test_service::shutdown_test_service() :
    app_(vsomeip::runtime::get()->create_application("shutdown_test_service")), is_registered_(false), to_stop(false),
    offer_thread_(std::bind(&shutdown_test_service::run_test, this)) { }

TEST(someip_shutdown_test, receive_messages_send_shutdown_reply_and_immediately_stop) {
    shutdown_test_service test_service;
    if (test_service.init()) {
        test_service.start();
        test_service.join_offer_thread();
    }
}

std::uint32_t shutdown_test_service::size_buffer_ = 0;
bool shutdown_test_service::is_tcp_ = false;

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    if (argc > 1) {
        if (std::string("TCP") == std::string(argv[1])) {
            shutdown_test_service::size_buffer_ = shutdown_test::SHUTDOWN_SIZE_TCP;
            shutdown_test_service::is_tcp_ = true;
        } else if (std::string("UDP") == std::string(argv[1])) {
            shutdown_test_service::size_buffer_ = shutdown_test::SHUTDOWN_SIZE_UDP;
            shutdown_test_service::is_tcp_ = false;
        } else {
            shutdown_test_service::size_buffer_ = shutdown_test::SHUTDOWN_SIZE_UDS;
            shutdown_test_service::is_tcp_ = false;
        }
        if (std::string("FALSE") == std::string(argv[2])) {
            shutdown_test_service::size_buffer_ = 0;
        }
    }

    return test_main(argc, argv, std::chrono::seconds(30), true);
}
#endif
