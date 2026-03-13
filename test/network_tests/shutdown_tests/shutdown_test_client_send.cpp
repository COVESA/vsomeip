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

#include "common/test_main.hpp"

class shutdown_test_client {
public:
    shutdown_test_client();

    void run_test() {
        std::unique_lock<std::mutex> its_lock(mutex_);
        ASSERT_TRUE(condition_.wait_for(its_lock, std::chrono::seconds(10), [this] { return is_available_; }))
                << "Service not available in time";

        std::shared_ptr<vsomeip::payload> its_payload = vsomeip::runtime::get()->create_payload();
        std::vector<vsomeip::byte_t> its_payload_data;

        for (std::uint32_t i = 0; i < shutdown_test::SHUTDOWN_NUMBER_MESSAGES; ++i) {
            request_->set_service(shutdown_test::TEST_SERVICE_SERVICE_ID);
            request_->set_instance(shutdown_test::TEST_SERVICE_INSTANCE_ID);
            request_->set_method(shutdown_test::STOP_METHOD);
            request_->set_reliable(shutdown_test_client::is_tcp_);
            its_payload_data.assign(shutdown_test_client::size_buffer_, shutdown_test::DATA_CLIENT_TO_SERVICE);
            its_payload->set_data(its_payload_data);
            request_->set_payload(its_payload);

            app_->send(request_);
        }

        // magic sleep to give time for the last message to be read
        // in the router, before the clean-up starts the forceful stop
        // of the "server" connection within the router.
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        stop();
    }

    bool init() {
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return false;
        }

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

    static std::uint32_t size_buffer_;
    static bool is_tcp_;

private:
    std::shared_ptr<vsomeip::application> app_;
    std::shared_ptr<vsomeip::message> request_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool is_available_;
    std::thread sender_;
};

shutdown_test_client::shutdown_test_client() :
    app_(vsomeip::runtime::get()->create_application("shutdown_test_client")), request_(vsomeip::runtime::get()->create_request(true)),
    is_available_(false), sender_(std::bind(&shutdown_test_client::run_test, this)) { }

TEST(someip_shutdown_test, send_messages_to_service_and_immediately_stop) {
    shutdown_test_client test_client_;
    if (test_client_.init()) {
        test_client_.start();
        test_client_.join_sender_thread();
    }
}

std::uint32_t shutdown_test_client::size_buffer_ = 0;
bool shutdown_test_client::is_tcp_ = false;

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    if (argc > 1) {
        if (std::string("TCP") == std::string(argv[1])) {
            shutdown_test_client::size_buffer_ = shutdown_test::SHUTDOWN_SIZE_TCP;
            shutdown_test_client::is_tcp_ = true;
        } else if (std::string("UDP") == std::string(argv[1])) {
            shutdown_test_client::size_buffer_ = shutdown_test::SHUTDOWN_SIZE_UDP;
            shutdown_test_client::is_tcp_ = false;
        } else {
            shutdown_test_client::size_buffer_ = shutdown_test::SHUTDOWN_SIZE_UDS;
            shutdown_test_client::is_tcp_ = false;
        }
        if (std::string("FALSE") == std::string(argv[2])) {
            shutdown_test_client::size_buffer_ = 0;
        }
    }

    return test_main(argc, argv, std::chrono::seconds(30), true);
}
#endif
