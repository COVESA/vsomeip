// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <condition_variable>
#include <iomanip>
#include <mutex>
#include <thread>
#include <atomic>

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>
#include <vsomeip/internal/logger.hpp>

#include "suspend_resume_test.hpp"
#include "../someip_test_globals.hpp"
#include <common/vsomeip_app_utilities.hpp>
#include "common/test_main.hpp"

class suspend_resume_test_slave {
public:
    suspend_resume_test_slave() :

        app_(vsomeip::runtime::get()->create_application(name_)), started_{false},
        runner_(std::bind(&suspend_resume_test_slave::run, this)) { }

    void run_test() {

        VSOMEIP_DEBUG << "[TEST] Process: entry";

        register_state_handler();
        register_availability_handler();

        VSOMEIP_DEBUG << "[TEST] Process: start application";

        start();

        {
            std::unique_lock its_lock(availability_mutex_);
            VSOMEIP_DEBUG << "[TEST] Process: waiting service available";
            ASSERT_EQ(availability_cv_.wait_for(its_lock, std::chrono::seconds(10), [this]() { return is_available_.load(); }), true);
            VSOMEIP_INFO << "[TEST] Process: service available";
        }

        VSOMEIP_DEBUG << "[TEST] Process: start STR (suspend/resume) simulation";

        VSOMEIP_INFO << "[TEST] Process: suspend/resume, is_available=" << std::boolalpha << is_available_
                     << ", is_registered_=" << std::boolalpha << is_registered_;

        send_suspend();

        // Wait for availability to become false
        std::chrono::steady_clock::time_point unavailable_time;
        {
            std::unique_lock its_lock(availability_mutex_);
            VSOMEIP_DEBUG << "[TEST] Process: waiting availability=false event, is_available=" << std::boolalpha << is_available_.load();
            ASSERT_TRUE(availability_cv_.wait_for(its_lock, std::chrono::seconds(20), [this]() { return !is_available_.load(); }));
            unavailable_time = std::chrono::steady_clock::now();
            VSOMEIP_INFO << "[TEST] Process: received availability=false";
        }

        // Wait for availability to become true again
        std::chrono::steady_clock::time_point available_time;
        {
            std::unique_lock its_lock(availability_mutex_);
            VSOMEIP_DEBUG << "[TEST] Process: waiting availability=true event, is_available=" << std::boolalpha << is_available_.load();
            ASSERT_TRUE(availability_cv_.wait_for(its_lock, std::chrono::seconds(30), [this]() { return is_available_.load(); }));
            available_time = std::chrono::steady_clock::now();
            VSOMEIP_INFO << "[TEST] Process: received availability=true";
        }

        // Calculate time between unavailable and available
        auto suspension_duration = std::chrono::duration_cast<std::chrono::milliseconds>(available_time - unavailable_time);
        EXPECT_GT(suspension_duration.count(), 2000) << "[TEST] Process: StopOffer was only sent on resume";

        VSOMEIP_DEBUG << "[TEST] Process: successful, suspension duration=" << suspension_duration.count() << "ms";

        VSOMEIP_DEBUG << "[TEST] Process: done";

        send_stop();
        stop();

        VSOMEIP_DEBUG << "[TEST] Process: exit";
    }

private:
    void register_state_handler() {

        app_->register_state_handler(std::bind(&suspend_resume_test_slave::on_state, this, std::placeholders::_1));
    }

    void register_availability_handler() {

        app_->register_availability_handler(TEST_SERVICE, TEST_INSTANCE,
                                            std::bind(&suspend_resume_test_slave::on_availability, this, std::placeholders::_1,
                                                      std::placeholders::_2, std::placeholders::_3));
    }

    void start() {
        VSOMEIP_DEBUG << "[TEST] vSomeIP application: initialization";
        app_->init();

        started_ = true;
        cv_.notify_one();
    }

    void run() {
        VSOMEIP_DEBUG << "[TEST] vSomeIP application: waiting ready signal";

        {
            std::unique_lock its_lock(mutex_);
            cv_.wait(its_lock, [this] { return started_.load(); });
        }

        VSOMEIP_DEBUG << "[TEST] vSomeIP application: start";
        app_->start();

        VSOMEIP_DEBUG << "[TEST] vSomeIP application: exiting";
    }

    void stop() {

        app_->stop();
        runner_.join();
    }

    void on_state(vsomeip::state_type_e _state) {

        VSOMEIP_DEBUG << "[TEST] on_state registered=" << std::boolalpha << (_state == vsomeip::state_type_e::ST_REGISTERED)
                      << ", is_available=" << std::boolalpha << is_available_ << ", is_registered_=" << std::boolalpha << is_registered_;

        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            VSOMEIP_DEBUG << "[TEST] Request service " << std::hex << std::setfill('0') << std::setw(4) << TEST_SERVICE << "."
                          << std::setw(4) << TEST_INSTANCE;
            is_registered_ = true;
            app_->request_service(TEST_SERVICE, TEST_INSTANCE);
            VSOMEIP_DEBUG << "[TEST] Offer service " << std::hex << std::setfill('0') << std::setw(4) << TEST_SERVICE_SLAVE << "."
                          << std::setw(4) << TEST_INSTANCE << " (UDP+TCP)";
            app_->offer_service(TEST_SERVICE_SLAVE, TEST_INSTANCE, TEST_MAJOR, TEST_MINOR);
        } else {
            is_registered_ = false;
        }
    }

    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
        VSOMEIP_DEBUG << "[TEST] on_availability " << std::hex << std::setfill('0') << std::setw(4) << _service << "." << std::setw(4)
                      << _instance << ", is_available=" << std::boolalpha << _is_available << ", is_registered_=" << std::boolalpha
                      << is_registered_;

        if (_service == TEST_SERVICE && _instance == TEST_INSTANCE && _is_available != is_available_) {
            std::unique_lock its_lock(availability_mutex_);
            is_available_ = _is_available;
            availability_cv_.notify_one();
        }
    }

    void send_suspend() {
        VSOMEIP_DEBUG << "[TEST] Sending suspend message: " << std::hex << std::setfill('0') << std::setw(4) << TEST_SERVICE << "."
                      << std::setw(4) << TEST_INSTANCE << "." << std::setw(4) << TEST_METHOD << " " << std::setw(2) << TEST_SUSPEND;

        auto its_message = vsomeip::runtime::get()->create_request(false);
        its_message->set_service(TEST_SERVICE);
        its_message->set_instance(TEST_INSTANCE);
        its_message->set_method(TEST_METHOD);
        its_message->set_interface_version(TEST_MAJOR);
        its_message->set_message_type(vsomeip::message_type_e::MT_REQUEST_NO_RETURN);
        its_message->set_return_code(vsomeip::return_code_e::E_OK);

        vsomeip::byte_t its_data[] = {TEST_SUSPEND};
        auto its_payload = vsomeip::runtime::get()->create_payload();
        its_payload->set_data(its_data, sizeof(its_data));
        its_message->set_payload(its_payload);

        app_->send(its_message);
    }

    void send_stop() {
        VSOMEIP_DEBUG << "[TEST] Sending stop message: " << std::hex << std::setfill('0') << std::setw(4) << TEST_SERVICE << "."
                      << std::setw(4) << TEST_INSTANCE << "." << std::setw(4) << TEST_METHOD << " " << std::setw(2) << TEST_STOP;

        auto its_message = vsomeip::runtime::get()->create_request(false);
        its_message->set_service(TEST_SERVICE);
        its_message->set_instance(TEST_INSTANCE);
        its_message->set_method(TEST_METHOD);
        its_message->set_interface_version(TEST_MAJOR);
        its_message->set_message_type(vsomeip::message_type_e::MT_REQUEST_NO_RETURN);
        its_message->set_return_code(vsomeip::return_code_e::E_OK);

        vsomeip::byte_t its_data[] = {TEST_STOP};
        auto its_payload = vsomeip::runtime::get()->create_payload();
        its_payload->set_data(its_data, sizeof(its_data));
        its_message->set_payload(its_payload);

        app_->send(its_message);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

private: // members
    std::string name_;
    std::shared_ptr<vsomeip::application> app_;
    std::mutex mutex_;
    std::mutex availability_mutex_;
    std::condition_variable cv_;
    std::condition_variable availability_cv_;
    std::atomic<bool> started_;
    std::atomic<bool> is_available_{false};
    std::atomic<bool> is_registered_{false};
    std::thread runner_;
};

TEST(suspend_resume_test_connections, slave) {
    {
        suspend_resume_test_slave its_client;
        VSOMEIP_DEBUG << "[TEST] gtest: enter";
        its_client.run_test();
        VSOMEIP_DEBUG << "[TEST] gtest: exit";
    }

    // Thread sanitizer complains about logging without it
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    VSOMEIP_DEBUG << "[TEST] Starting Client";
    return test_main(argc, argv, std::chrono::seconds(120));
}
#endif
