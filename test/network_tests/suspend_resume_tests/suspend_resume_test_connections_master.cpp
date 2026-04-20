// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <signal.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>
#include <vsomeip/internal/logger.hpp>

#include "suspend_resume_test.hpp"
#include "../someip_test_globals.hpp"
#include <common/vsomeip_app_utilities.hpp>
#include "common/test_main.hpp"

// 169.254.87.2 → little-endian hex on x86: 0x0257FEA9
constexpr auto BOARDNET_IP_HEX = "0257FEA9";

// Returns the number of UDP sockets in /proc/net/udp whose local address
// matches `BOARDNET_IP_HEX` (8 upper-case hex digits, little-endian, e.g. "0257FEA9"
// for 169.254.87.2 on x86), ignoring the wildcard address "00000000".
// Returns -1 if /proc/net/udp cannot be opened.
static int count_udp_sockets_on_ip() {
    std::ifstream f("/proc/net/udp");
    if (!f.is_open())
        return -1;

    int count = 0;
    std::string line;
    std::getline(f, line); // skip header
    while (std::getline(f, line)) {
        std::istringstream iss(line);
        std::string sl, local;
        iss >> sl >> local;
        // local is "XXXXXXXX:PPPP"; ignore wildcard 00000000
        if (local.size() >= 8 && local.substr(0, 8) == BOARDNET_IP_HEX)
            ++count;
    }
    return count;
}

// Polls /proc/net/udp until no sockets remain bound to `BOARDNET_IP_HEX`,
// or until `timeout` elapses. Returns true when the count reaches 0.
static bool wait_for_udp_sockets_closed(std::chrono::milliseconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    do {
        int n = count_udp_sockets_on_ip();
        if (n == 0)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}

// Returns the number of TCP connections in /proc/net/tcp whose local address
// matches `BOARDNET_IP_HEX` and whose state is ESTABLISHED (01) or CLOSE_WAIT (08),
// ignoring privileged ports (< 1024) which belong to infrastructure (e.g. SSH).
// vsomeip always binds reliable endpoints on unprivileged ports (>= 1024).
// TIME_WAIT (06) is intentionally ignored — it is managed by the kernel and
// means the socket was already properly closed by the application.
// Returns -1 if /proc/net/tcp cannot be opened.
static int count_tcp_active_connections_on_ip() {
    std::ifstream f("/proc/net/tcp");
    if (!f.is_open())
        return -1;

    int count = 0;
    std::string line;
    std::getline(f, line); // skip header
    while (std::getline(f, line)) {
        std::istringstream iss(line);
        std::string sl, local, remote, state;
        iss >> sl >> local >> remote >> state;
        // local is "XXXXXXXX:PPPP"
        if (local.size() < 13 || local.substr(0, 8) != BOARDNET_IP_HEX)
            continue;
        // Skip infrastructure ports (SSH=22, etc.) — vsomeip uses ports >= 1024
        unsigned long port = std::stoul(local.substr(9, 4), nullptr, 16);
        if (port < 1024)
            continue;
        // state "01"=ESTABLISHED, "08"=CLOSE_WAIT
        if (state == "01" || state == "08")
            ++count;
    }
    return count;
}

// Polls /proc/net/tcp until no ESTABLISHED or CLOSE_WAIT connections remain
// on `BOARDNET_IP_HEX`, or until `timeout` elapses. Returns true when the count reaches 0.
static bool wait_for_tcp_connections_closed(std::chrono::milliseconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    do {
        int n = count_tcp_active_connections_on_ip();
        if (n == 0)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}

class suspend_resume_test_master {
public:
    suspend_resume_test_master() : app_(vsomeip::runtime::get()->create_application(name_)) {
        VSOMEIP_INFO << "[TEST] Create test object";
        sr_runner_ = std::thread(std::bind(&suspend_resume_test_master::sr_run, this));
    }

    void run_test() {

        register_state_handler();
        register_availability_handler();
        register_message_handler();
        register_subscription_handler();

        VSOMEIP_DEBUG << "[TEST] Process: start application";

        start();

        {
            VSOMEIP_DEBUG << "[TEST] Process: waiting test end";

            std::unique_lock its_lock(mutex_);
            EXPECT_EQ(cv_.wait_for(its_lock, std::chrono::seconds(30)), std::cv_status::no_timeout);
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

        // Wait until the master has an active connection to TEST_SERVICE_SLAVE (offered by the slave with UDP+TCP) to increase the number
        // of active sockets present before the suspend check.
        {
            std::unique_lock its_lock(availability_mutex_);
            VSOMEIP_DEBUG << "[TEST] Process: waiting service available";
            ASSERT_EQ(availability_cv_.wait_for(its_lock, std::chrono::seconds(10), [this]() { return is_available_.load(); }), true);
            VSOMEIP_INFO << "[TEST] Process: service available";
        }

        {
            VSOMEIP_DEBUG << "[TEST] STR simulation: waiting signal";
            std::unique_lock its_lock(sr_mutex_);
            sr_cv_.wait(its_lock, [this] { return is_suspend_requested_.load() || !is_running_; });
            is_suspend_requested_ = false;
        }

        if (!is_running_) {
            return;
        }

        VSOMEIP_DEBUG << "[TEST] STR simulation: is_running_=" << std::boolalpha << is_running_ << ", is_subscribe=" << std::boolalpha
                      << is_subscribe_;

        VSOMEIP_INFO << "[TEST] STR simulation: Trigger SUSPEND (set_routing_state to RS_SUSPENDED), is_subscribe=" << std::boolalpha
                     << is_subscribe_ << ", is_running_=" << std::boolalpha << is_running_
                     << ", udp connections on boardnet=" << count_udp_sockets_on_ip()
                     << ", tcp connections on boardnet=" << count_tcp_active_connections_on_ip();
        app_->set_routing_state(vsomeip::routing_state_e::RS_SUSPENDED);

        EXPECT_TRUE(wait_for_udp_sockets_closed(std::chrono::milliseconds(100)))
                << "[TEST] External UDP sockets on boardnet were not closed on RS_SUSPENDED"
                << " (remaining=" << count_udp_sockets_on_ip() << ")";

        EXPECT_TRUE(wait_for_tcp_connections_closed(std::chrono::milliseconds(100)))
                << "[TEST] External TCP connections on boardnet were not closed on RS_SUSPENDED"
                << " (remaining=" << count_tcp_active_connections_on_ip() << ")";

        VSOMEIP_DEBUG << "[TEST] STR simulation: simulate network off, is_subscribe=" << std::boolalpha << is_subscribe_
                      << ", is_running_=" << std::boolalpha << is_running_;

        std::ignore = system("ip link set eth0 down");

        std::this_thread::sleep_for(std::chrono::seconds(3));

        VSOMEIP_DEBUG << "[TEST] STR simulation: simulate network on, is_subscribe=" << std::boolalpha << is_subscribe_
                      << ", is_running_=" << std::boolalpha << is_running_;

        std::ignore = system("ip link set eth0 up");
        std::ignore = system("ip addr add 169.254.87.2/24 dev eth0");
        std::ignore = system("ip route add 169.254.87.0/24 src 169.254.87.2 dev eth0");
        std::ignore = system("ip route add 224.0.0.0/4 dev eth0");

        VSOMEIP_INFO << "[TEST] STR simulation: Trigger RESUME (set_routing_state to RS_RESUMED), is_subscribe=" << std::boolalpha
                     << is_subscribe_ << ", is_running_=" << std::boolalpha << is_running_;
        app_->set_routing_state(vsomeip::routing_state_e::RS_RESUMED);

        VSOMEIP_DEBUG << "[TEST] STR simulation: exit, is_running_=" << std::boolalpha << is_running_ << ", is_subscribe=" << std::boolalpha
                      << is_subscribe_ << ", is_suspend_requested_=" << std::boolalpha << is_suspend_requested_;
        wait_runner_.store(false);
    }

    void register_state_handler() {

        app_->register_state_handler(std::bind(&suspend_resume_test_master::on_state, this, std::placeholders::_1));
    }

    void register_availability_handler() {

        app_->register_availability_handler(TEST_SERVICE_SLAVE, TEST_INSTANCE,
                                            std::bind(&suspend_resume_test_master::on_availability_service, this, std::placeholders::_1,
                                                      std::placeholders::_2, std::placeholders::_3));
    }

    void register_message_handler() {

        app_->register_message_handler(TEST_SERVICE, TEST_INSTANCE, TEST_METHOD,
                                       std::bind(&suspend_resume_test_master::on_message, this, std::placeholders::_1));
    }

    void register_subscription_handler() {

        app_->register_subscription_handler(TEST_SERVICE, TEST_INSTANCE, TEST_EVENTGROUP,
                                            std::bind(&suspend_resume_test_master::on_subscribe, this, std::placeholders::_1,
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
            VSOMEIP_DEBUG << "[TEST] Request service " << std::hex << std::setfill('0') << std::setw(4) << TEST_SERVICE_SLAVE << "."
                          << std::setw(4) << TEST_INSTANCE << " (client's UDP+TCP service)";
            app_->request_service(TEST_SERVICE_SLAVE, TEST_INSTANCE);
        }
    }

    void on_availability_service(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
        VSOMEIP_DEBUG << "[TEST] on_availability " << std::hex << std::setfill('0') << std::setw(4) << _service << "." << std::setw(4)
                      << _instance << ", is_available=" << std::boolalpha << _is_available;
        if (_service == TEST_SERVICE_SLAVE && _instance == TEST_INSTANCE && _is_available != is_available_) {
            std::unique_lock its_lock(availability_mutex_);
            is_available_ = _is_available;
            availability_cv_.notify_one();
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
    std::atomic<bool> is_available_{false};
    std::mutex mutex_;
    std::condition_variable cv_;
    std::mutex sr_mutex_;
    std::condition_variable sr_cv_;
    std::mutex availability_mutex_;
    std::condition_variable availability_cv_;
    std::thread runner_;
    std::thread sr_runner_;
};

TEST(suspend_resume_test_connections, master) {
    {
        suspend_resume_test_master its_service;
        VSOMEIP_DEBUG << "[TEST] gtest: enter";
        its_service.run_test();
        VSOMEIP_DEBUG << "[TEST] gtest: exit";
    }

    // Thread sanitizer complains about logging without it
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    return test_main(argc, argv, std::chrono::seconds(60));
}
#endif
