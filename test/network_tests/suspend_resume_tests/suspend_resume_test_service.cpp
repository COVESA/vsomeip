// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <signal.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <iomanip>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>
#include <vsomeip/internal/logger.hpp>

#include "suspend_resume_test.hpp"
#include "../someip_test_globals.hpp"
#include <common/vsomeip_app_utilities.hpp>

pid_t daemon_pid__;

class suspend_resume_test_service : public vsomeip_utilities::base_logger {
public:
    suspend_resume_test_service() :
        vsomeip_utilities::base_logger("ATCA", "APPLICATION TEST CLIENT AVAILABILITY"), name_("suspend_resume_test_service"),
        app_(vsomeip::runtime::get()->create_application(name_)) {
        VSOMEIP_INFO << "[TEST] Create test object";
        sr_runner_ = std::thread(std::bind(&suspend_resume_test_service::sr_run, this));
    }

    void run_test() {

        VSOMEIP_DEBUG << "[TEST] Process: entry, daemon with pid=" << std::dec << daemon_pid__;

        register_state_handler();
        register_message_handler();
        register_subscription_handler();

        VSOMEIP_DEBUG << "[TEST] Process: start application";

        start();

        {
            VSOMEIP_DEBUG << "[TEST] Process: waiting test end";

            std::unique_lock<std::mutex> its_lock(mutex_);
            EXPECT_EQ(cv_.wait_for(its_lock, std::chrono::seconds(120)), std::cv_status::no_timeout);
        }

        VSOMEIP_DEBUG << "[TEST] Process: Done";

        stop();

        VSOMEIP_DEBUG << "[TEST] Process: Exit";
    }

private:
    void start() {

        VSOMEIP_DEBUG << "[TEST] vSomeIP application: initialization";
        app_->init();

        runner_ = std::thread([this]() {
            VSOMEIP_DEBUG << "[TEST] vSomeIP application: start";
            app_->start();
            VSOMEIP_DEBUG << "[TEST] vSomeIP application: exiting";
        });
    }

    void stop() {

        is_running_ = false;

        do {
            VSOMEIP_DEBUG << "[TEST] Wait runner: " << std::boolalpha << wait_runner_.load();

            {
                std::scoped_lock its_lock(sr_mutex_);
                sr_cv_.notify_one();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while (wait_runner_.load());

        app_->stop();

        runner_.join();
        sr_runner_.join();
    }

    void sr_run() {

        VSOMEIP_DEBUG << "[TEST] STR simulation: enter, is_running_=" << std::boolalpha << is_running_;

        for (unsigned iteration = 1; is_running_; ++iteration) {
            {
                VSOMEIP_DEBUG << "[TEST] STR simulation: waiting signal, iteration#" << iteration;
                std::unique_lock<std::mutex> its_lock(sr_mutex_);
                sr_cv_.wait(its_lock, [this] { return is_suspend_requested_.load() || !is_running_; });
                is_suspend_requested_ = false;
            }

            if (!is_running_) {
                break;
            }

            unsigned delay_ms = (iteration * 1049) % 773;

            VSOMEIP_DEBUG << "[TEST] STR simulation: iteration#" << std::dec << iteration << ", is_running_=" << std::boolalpha
                          << is_running_ << ", delay_ms=" << delay_ms << ", is_subscribe=" << std::boolalpha << is_subscribe_;

            if (iteration == 1) {
                VSOMEIP_INFO << "[TEST] STR simulation: iteration#" << std::dec << iteration
                             << ", skip kill SIGUSR1, is_subscribe=" << std::boolalpha << is_subscribe_
                             << ", is_running_=" << std::boolalpha << is_running_;
            } else {
                VSOMEIP_INFO << "[TEST] STR simulation: iteration#" << std::dec << iteration << ", send kill SIGUSR1 to PID: " << std::dec
                             << daemon_pid__ << ", is_subscribe=" << std::boolalpha << is_subscribe_ << ", is_running_=" << std::boolalpha
                             << is_running_;
                kill(daemon_pid__, SIGUSR1);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));

            VSOMEIP_DEBUG << "[TEST] STR simulation: iteration#" << std::dec << iteration
                          << ", simulate network off, is_subscribe=" << std::boolalpha << is_subscribe_
                          << ", is_running_=" << std::boolalpha << is_running_;

            std::ignore = system("timeout 2s ip link set eth0 down");

            std::this_thread::sleep_for(std::chrono::seconds(2));

            if ((iteration % 3) == 0) {
                VSOMEIP_INFO << "[TEST] STR simulation: iteration#" << std::dec << iteration
                             << ", send kill SIGUSR2 too early to PID: " << std::dec << daemon_pid__ << ", before " << (1000 - delay_ms)
                             << "ms, is_subscribe=" << std::boolalpha << is_subscribe_ << ", is_running_=" << std::boolalpha << is_running_;
                kill(daemon_pid__, SIGUSR2);
            }

            VSOMEIP_DEBUG << "[TEST] STR simulation: iteration#" << std::dec << iteration
                          << ", simulate network on, is_subscribe=" << std::boolalpha << is_subscribe_ << ", is_running_=" << std::boolalpha
                          << is_running_;

            std::ignore = system("timeout 2s ip link set eth0 up");
            std::ignore = system("timeout 2s ip addr add 169.254.87.2/24 dev eth0");
            std::ignore = system("timeout 2s ip route add 169.254.87.0/24 src 169.254.87.2 dev eth0");
            std::ignore = system("timeout 2s ip route add 224.0.0.0/4 dev eth0");

            std::this_thread::sleep_for(std::chrono::milliseconds(1000 - delay_ms));

            if ((iteration % 3) != 0) {
                VSOMEIP_INFO << "[TEST] STR simulation: iteration#" << std::dec << iteration << ", send kill SIGUSR2 to PID: " << std::dec
                             << daemon_pid__ << ", after " << (1000 - delay_ms) << "ms, is_subscribe=" << std::boolalpha << is_subscribe_
                             << ", is_running_=" << std::boolalpha << is_running_;
                kill(daemon_pid__, SIGUSR2);
            }
        }

        // Workaround with logging which is freed before all threads have been stopped
        kill(daemon_pid__, SIGUSR1);

        VSOMEIP_DEBUG << "[TEST] STR simulation: exit, is_running_=" << std::boolalpha << is_running_ << ", is_subscribe=" << std::boolalpha
                      << is_subscribe_ << ", is_suspend_requested_=" << std::boolalpha << is_suspend_requested_;
        wait_runner_.store(false);
    }

    void register_state_handler() {

        app_->register_state_handler(std::bind(&suspend_resume_test_service::on_state, this, std::placeholders::_1));
    }

    void register_message_handler() {

        app_->register_message_handler(TEST_SERVICE, TEST_INSTANCE, TEST_METHOD,
                                       std::bind(&suspend_resume_test_service::on_message, this, std::placeholders::_1));
    }

    void register_subscription_handler() {

        app_->register_subscription_handler(TEST_SERVICE, TEST_INSTANCE, TEST_EVENTGROUP,
                                            std::bind(&suspend_resume_test_service::on_subscribe, this, std::placeholders::_1,
                                                      std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
    }

    void offer_service() {
        VSOMEIP_DEBUG << "[TEST] Offer event " << std::hex << std::setfill('0') << std::setw(4) << TEST_SERVICE << "." << std::setw(4)
                      << TEST_INSTANCE << "." << std::setw(4) << TEST_EVENT << "." << std::setw(2) << TEST_EVENTGROUP
                      << ", is_subscribe=" << std::boolalpha << is_subscribe_;

        app_->offer_event(TEST_SERVICE, TEST_INSTANCE, TEST_EVENT, {TEST_EVENTGROUP}, vsomeip::event_type_e::ET_FIELD,
                          std::chrono::milliseconds::zero(), false, true, nullptr, vsomeip::reliability_type_e::RT_UNRELIABLE);

        vsomeip::byte_t its_data[] = {0x1, 0x2, 0x3};
        auto its_payload = vsomeip::runtime::get()->create_payload();
        its_payload->set_data(its_data, sizeof(its_data));

        VSOMEIP_DEBUG << "[TEST] Notify event " << std::hex << std::setfill('0') << std::setw(4) << TEST_SERVICE << "." << std::setw(4)
                      << TEST_INSTANCE << "." << std::setw(4) << TEST_EVENT << " " << std::setw(2) << its_data[0] << std::setw(2) << " "
                      << its_data[1] << " " << std::setw(2) << its_data[2] << ", is_subscribe=" << std::boolalpha << is_subscribe_;

        app_->notify(TEST_SERVICE, TEST_INSTANCE, TEST_EVENT, its_payload);

        VSOMEIP_DEBUG << "[TEST] Offer service " << std::hex << std::setfill('0') << std::setw(4) << TEST_SERVICE << "." << std::setw(4)
                      << TEST_INSTANCE << "." << std::setw(2) << TEST_MAJOR << ", minor=" << std::dec << std::setw(0) << TEST_MINOR
                      << ", is_subscribe=" << std::boolalpha << is_subscribe_;

        app_->offer_service(TEST_SERVICE, TEST_INSTANCE, TEST_MAJOR, TEST_MINOR);
    }

    // handler
    void on_state(vsomeip::state_type_e _state) {
        VSOMEIP_DEBUG << "[TEST] on_state registered=" << std::boolalpha << (_state == vsomeip::state_type_e::ST_REGISTERED)
                      << ", is_subscribe=" << std::boolalpha << is_subscribe_;

        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            offer_service();
        }
    }

    void on_message(const std::shared_ptr<vsomeip::message>& _message) {
        VSOMEIP_DEBUG << "[TEST] on_message " << std::hex << std::setfill('0') << std::setw(4) << _message->get_service() << "."
                      << std::setw(4) << _message->get_instance() << "." << std::setw(4) << _message->get_method() << " " << std::setw(2)
                      << (_message->get_payload()->get_length() > 0 ? *_message->get_payload()->get_data() : -1) << ", length=" << std::dec
                      << std::setw(4) << _message->get_payload()->get_length() << ", is_subscribe=" << std::boolalpha << is_subscribe_;

        if (_message->get_service() == TEST_SERVICE && _message->get_instance() == TEST_INSTANCE && _message->get_method() == TEST_METHOD) {

            if (_message->get_payload()->get_length() == 1) {

                vsomeip::byte_t its_control_byte(*_message->get_payload()->get_data());

                switch (its_control_byte) {
                case TEST_SUSPEND: {
                    VSOMEIP_INFO << "[TEST] STR simulation: trigger";
                    std::scoped_lock its_lock(sr_mutex_);
                    is_suspend_requested_ = true;
                    sr_cv_.notify_one();
                } break;
                case TEST_STOP: {
                    VSOMEIP_INFO << "[TEST] Request: stop";
                    std::scoped_lock its_lock(mutex_);
                    cv_.notify_one();
                } break;
                default:;
                }
            }
        }
    }

    bool on_subscribe(vsomeip::client_t _client, vsomeip::uid_t _uid, vsomeip::gid_t _gid, bool _is_subscribe) {

        (void)_client;
        (void)_uid;
        (void)_gid;

        VSOMEIP_DEBUG << "[TEST] on_subscribe client=" << std::hex << std::setfill('0') << std::setw(4) << _client
                      << ", subscribe=" << std::boolalpha << _is_subscribe << ", was is_subscribe_=" << is_subscribe_;
        is_subscribe_ = _is_subscribe;

        return true;
    }

private: // members
    std::string name_;
    std::shared_ptr<vsomeip::application> app_;
    std::atomic<bool> is_running_{true};
    std::atomic<bool> wait_runner_{true};
    std::atomic<bool> is_subscribe_{false};
    std::atomic<bool> is_suspend_requested_{false};
    std::mutex mutex_;
    std::condition_variable cv_;
    std::mutex sr_mutex_;
    std::condition_variable sr_cv_;
    std::thread runner_;
    std::thread sr_runner_;
};

TEST(suspend_resume_test, fast) {
    {
        suspend_resume_test_service its_service;
        VSOMEIP_DEBUG << "[TEST] gtest: enter";
        its_service.run_test();
        VSOMEIP_DEBUG << "[TEST] gtest: exit";
    }

    // Thread sanitizer complains about logging without it
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {

    ::testing::InitGoogleTest(&argc, argv);

    daemon_pid__ = atoi(argv[1]);

    return RUN_ALL_TESTS();
}
#endif
