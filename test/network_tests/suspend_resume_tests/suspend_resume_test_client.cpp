// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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

class suspend_resume_test_client : public vsomeip_utilities::base_logger {
public:
    suspend_resume_test_client() :
        vsomeip_utilities::base_logger("SRTC", "SUSPEND RESUME TEST CLIENT"), name_("suspend_resume_test_client"),
        app_(vsomeip::runtime::get()->create_application(name_)), started_{false}, has_received_(false),
        runner_(std::bind(&suspend_resume_test_client::run, this)) { }

    void run_test() {

        VSOMEIP_DEBUG << "[TEST] Process: entry";

        register_state_handler();
        register_message_handler();
        register_availability_handler();

        VSOMEIP_DEBUG << "[TEST] Process: start application";

        start();

        {
            std::unique_lock<std::mutex> its_lock(availability_mutex_);
            auto r = availability_cv_.wait_for(its_lock, std::chrono::seconds(10));
            ASSERT_EQ(r, std::cv_status::no_timeout);
            VSOMEIP_DEBUG << "[TEST] Process: service available";
        }

        VSOMEIP_DEBUG << "[TEST] Process: toggle subscriptions";

        toggle();

        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            if (!has_received_) {
                auto r = cv_.wait_for(its_lock, std::chrono::seconds(10));
                ASSERT_EQ(r, std::cv_status::no_timeout);
                VSOMEIP_DEBUG << "[TEST] Process: notification received";
            } else {
                VSOMEIP_DEBUG << "[TEST] Process: notification already received";
            }
        }

        VSOMEIP_DEBUG << "[TEST] Process: start STR (suspend/resume) simulation";

        for (int i = 1; i <= 6; ++i) {
            VSOMEIP_INFO << "[TEST] Process: suspend/resume, iteration#" << std::dec << i << ", is_available=" << std::boolalpha
                         << is_available_ << ", has_received=" << std::boolalpha << has_received_ << ", is_registered_=" << std::boolalpha
                         << is_registered_;

            auto begin_time_point = std::chrono::steady_clock::now();

            send_suspend();

            // Wait for availability to become false
            std::chrono::steady_clock::time_point unavailable_time;
            {
                std::unique_lock<std::mutex> its_lock(availability_mutex_);
                // Ensure that was_unavailable_ is only set in this iteration instead of coming already set from the previous iteration
                was_unavailable_ = false;
                VSOMEIP_DEBUG << "[TEST] Process: waiting availability=false event, iteration#" << std::dec << i
                              << ", is_available=" << std::boolalpha << is_available_.load();
                ASSERT_TRUE(availability_cv_.wait_for(its_lock, std::chrono::seconds(20),
                                                      [this]() { return !is_available_.load() || was_unavailable_.load(); }));
                unavailable_time = std::chrono::steady_clock::now();
                VSOMEIP_INFO << "[TEST] Process: received availability=false, iteration#" << std::dec << i;
            }

            // Wait for availability to become true again
            std::chrono::steady_clock::time_point available_time;
            {
                std::unique_lock<std::mutex> its_lock(availability_mutex_);
                VSOMEIP_DEBUG << "[TEST] Process: waiting availability=true event, iteration#" << std::dec << i
                              << ", is_available=" << std::boolalpha << is_available_.load();
                ASSERT_TRUE(availability_cv_.wait_for(its_lock, std::chrono::seconds(20), [this]() { return is_available_.load(); }));
                available_time = std::chrono::steady_clock::now();
                VSOMEIP_INFO << "[TEST] Process: received availability=true, iteration#" << std::dec << i;
            }

            // Calculate time between unavailable and available
            auto suspension_duration = std::chrono::duration_cast<std::chrono::milliseconds>(available_time - unavailable_time);
            if (i > 1) {
                EXPECT_GT(suspension_duration.count(), 2000)
                        << "[TEST] Process: iteration#" << std::dec << i << " - StopOffer was only sent on resume";
            }

            // Wait for event value notification
            {
                std::unique_lock<std::mutex> its_lock(mutex_);
                VSOMEIP_DEBUG << "[TEST] Process: waiting event value notification, iteration#" << std::dec << i
                              << ", is_available=" << std::boolalpha << is_available_;
                ASSERT_EQ(cv_.wait_for(its_lock, std::chrono::seconds(20)), std::cv_status::no_timeout);
            }

            auto end_time_point = std::chrono::steady_clock::now();

            VSOMEIP_DEBUG << "[TEST] Process: iteration#" << std::dec << i << " successful, performed in "
                          << std::chrono::duration_cast<std::chrono::milliseconds>(end_time_point - begin_time_point).count() << "ms"
                          << ", is_available=" << std::boolalpha << is_available_.load();
        }

        VSOMEIP_DEBUG << "[TEST] Process: done";

        send_stop();
        stop();

        VSOMEIP_DEBUG << "[TEST] Process: exit";
    }

private:
    void register_state_handler() {

        app_->register_state_handler(std::bind(&suspend_resume_test_client::on_state, this, std::placeholders::_1));
    }

    void register_availability_handler() {

        app_->register_availability_handler(TEST_SERVICE, TEST_INSTANCE,
                                            std::bind(&suspend_resume_test_client::on_availability, this, std::placeholders::_1,
                                                      std::placeholders::_2, std::placeholders::_3));
    }

    void register_message_handler() {

        app_->register_message_handler(TEST_SERVICE, TEST_INSTANCE, TEST_EVENT,
                                       std::bind(&suspend_resume_test_client::on_message, this, std::placeholders::_1));
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
            std::unique_lock<std::mutex> its_lock(mutex_);
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
                      << ", is_available=" << std::boolalpha << is_available_ << ", has_received=" << std::boolalpha << has_received_
                      << ", is_registered_=" << std::boolalpha << is_registered_;

        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            VSOMEIP_DEBUG << "[TEST] Request service/event " << std::hex << std::setfill('0') << std::setw(4) << TEST_SERVICE << "."
                          << std::setw(4) << TEST_INSTANCE << "." << std::setw(4) << TEST_EVENT << "." << std::setw(2) << TEST_EVENTGROUP;
            is_registered_ = true;
            app_->request_event(TEST_SERVICE, TEST_INSTANCE, TEST_EVENT, {TEST_EVENTGROUP});
            app_->request_service(TEST_SERVICE, TEST_INSTANCE);
        } else {
            is_registered_ = false;
        }
    }

    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
        VSOMEIP_DEBUG << "[TEST] on_availability " << std::hex << std::setfill('0') << std::setw(4) << _service << "." << std::setw(4)
                      << _instance << ", is_available=" << std::boolalpha << _is_available << ", was available=" << is_available_
                      << ", has_received=" << std::boolalpha << has_received_ << ", is_registered_=" << std::boolalpha << is_registered_;

        if (_service == TEST_SERVICE && _instance == TEST_INSTANCE && _is_available != is_available_) {
            std::unique_lock<std::mutex> its_lock(availability_mutex_);

            if (_is_available) {
                VSOMEIP_DEBUG << "[TEST] Availability triggers signal";
                if (!is_available_) {
                    was_unavailable_ = true;
                }
            } else {
                VSOMEIP_DEBUG << "[TEST] Unavailability clears flag";
                has_received_ = false;
            }

            availability_cv_.notify_one();
            is_available_ = _is_available;
        }
    }

    void on_message(const std::shared_ptr<vsomeip::message>& _message) {
        VSOMEIP_DEBUG << "[TEST] on_message " << std::hex << std::setfill('0') << std::setw(4) << _message->get_service() << "."
                      << std::setw(4) << _message->get_instance() << "." << std::setw(4) << _message->get_method() << " " << std::setw(2)
                      << (_message->get_payload()->get_length() > 0 ? *_message->get_payload()->get_data() : -1) << ", length=" << std::dec
                      << std::setw(4) << _message->get_payload()->get_length() << ", is_available=" << std::boolalpha << is_available_
                      << ", has_received=" << std::boolalpha << has_received_ << ", is_registered_=" << std::boolalpha << is_registered_;

        if (_message->get_service() == TEST_SERVICE && _message->get_instance() == TEST_INSTANCE && _message->get_method() == TEST_EVENT) {
            std::unique_lock<std::mutex> its_lock(mutex_);

            if (!is_available_) {
                VSOMEIP_ERROR << "[TEST] Notification ignored because received out-of-order";
            } else if (!has_received_) {
                VSOMEIP_DEBUG << "[TEST] Notification sets flag and triggers signal";
                has_received_ = true;
                cv_.notify_one();
            }
        }
    }

    void toggle() {
        VSOMEIP_DEBUG << "[TEST] Subscribe to " << std::hex << std::setfill('0') << std::setw(4) << TEST_SERVICE << "." << std::setw(4)
                      << TEST_INSTANCE << "." << std::setw(2) << TEST_EVENTGROUP << "." << std::setw(2) << TEST_MAJOR;
        app_->subscribe(TEST_SERVICE, TEST_INSTANCE, TEST_EVENTGROUP, TEST_MAJOR);
        std::this_thread::sleep_for(std::chrono::seconds(3));
        VSOMEIP_DEBUG << "[TEST] Toggle subscription to " << std::hex << std::setfill('0') << std::setw(4) << TEST_SERVICE << "."
                      << std::setw(4) << TEST_INSTANCE << "." << std::setw(2) << TEST_EVENTGROUP << "." << std::setw(2) << TEST_MAJOR;
        app_->unsubscribe(TEST_SERVICE, TEST_INSTANCE, TEST_EVENTGROUP);
        app_->subscribe(TEST_SERVICE, TEST_INSTANCE, TEST_EVENTGROUP, TEST_MAJOR);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        VSOMEIP_DEBUG << "[TEST] Toggle subscription to " << std::hex << std::setfill('0') << std::setw(4) << TEST_SERVICE << "."
                      << std::setw(4) << TEST_INSTANCE << "." << std::setw(2) << TEST_EVENTGROUP << "." << std::setw(2) << TEST_MAJOR;
        app_->unsubscribe(TEST_SERVICE, TEST_INSTANCE, TEST_EVENTGROUP);
        app_->subscribe(TEST_SERVICE, TEST_INSTANCE, TEST_EVENTGROUP, TEST_MAJOR);
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
    std::atomic<bool> has_received_{false};
    std::atomic<bool> is_available_{false};
    std::atomic<bool> is_registered_{false};
    std::atomic<bool> was_unavailable_{false};
    std::thread runner_;
};

TEST(suspend_resume_test, fast) {
    {
        suspend_resume_test_client its_client;
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
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
#endif
