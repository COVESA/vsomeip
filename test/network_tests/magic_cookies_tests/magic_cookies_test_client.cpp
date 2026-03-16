// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>

#include "../someip_test_globals.hpp"
#include <common/vsomeip_app_utilities.hpp>
#include "common/test_main.hpp"

#define private public
#define protected public

#include "../implementation/runtime/include/application_impl.hpp"
#include "../implementation/routing/include/routing_manager_impl.hpp"
#include "../implementation/endpoints/include/tcp_client_endpoint_impl.hpp"
#include "../implementation/routing/include/routing_manager.hpp"
#include "../implementation/runtime/include/routing_application.hpp"

using namespace std::chrono_literals;

class magic_cookies_test_client {
public:
    magic_cookies_test_client() :
        app_(new vsomeip::application_impl("", "")), is_blocked_(false), received_responses_(0), received_errors_(0),
        runner_(std::bind(&magic_cookies_test_client::run, this)) { }

    void init() {
        VSOMEIP_INFO << "Initializing...";
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            exit(EXIT_FAILURE);
        }

        app_->register_state_handler(std::bind(&magic_cookies_test_client::on_state, this, std::placeholders::_1));

        app_->register_message_handler(vsomeip::ANY_SERVICE, vsomeip_test::TEST_SERVICE_INSTANCE_ID, vsomeip::ANY_METHOD,
                                       std::bind(&magic_cookies_test_client::on_message, this, std::placeholders::_1));

        app_->register_availability_handler(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID,
                                            std::bind(&magic_cookies_test_client::on_availability, this, std::placeholders::_1,
                                                      std::placeholders::_2, std::placeholders::_3),
                                            vsomeip::DEFAULT_MAJOR, vsomeip::DEFAULT_MINOR);
    }

    void start() {
        VSOMEIP_INFO << "Starting...";
        app_->start();
    }

    void stop() {
        VSOMEIP_INFO << "Stopping...";
        app_->clear_all_handler();
        app_->stop();
    }

    void on_state(vsomeip::state_type_e _state) {
        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            VSOMEIP_INFO << "Client registration done.";
            app_->request_service(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID, vsomeip::ANY_MAJOR,
                                  vsomeip::ANY_MINOR);
        }
    }

    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
        VSOMEIP_INFO << "Service [" << std::hex << std::setfill('0') << std::setw(4) << _service << "." << _instance << "] is "
                     << (_is_available ? "available." : "NOT available.");

        if (vsomeip_test::TEST_SERVICE_SERVICE_ID == _service && vsomeip_test::TEST_SERVICE_INSTANCE_ID == _instance) {
            if (_is_available) {
                std::scoped_lock its_lock(mutex_);
                is_blocked_ = true;
                condition_.notify_one();
            }
        }
    }

    void on_message(const std::shared_ptr<vsomeip::message>& _response) {
        std::scoped_lock its_lock(mutex_);
        if (_response->get_return_code() == vsomeip::return_code_e::E_OK) {
            VSOMEIP_INFO << "Received a response from Service [" << std::hex << std::setfill('0') << std::setw(4)
                         << _response->get_service() << '.' << std::setw(4) << _response->get_instance() << "] to Client/Session ["
                         << std::setw(4) << _response->get_client() << '/' << std::setw(4) << _response->get_session() << ']';
            received_responses_++;
        } else if (_response->get_return_code() == vsomeip::return_code_e::E_MALFORMED_MESSAGE) {
            VSOMEIP_INFO << "Received an error message from Service [" << std::hex << std::setfill('0') << std::setw(4)
                         << _response->get_service() << '.' << std::setw(4) << _response->get_instance() << "] to Client/Session ["
                         << std::setw(4) << _response->get_client() << '/' << std::setw(4) << _response->get_session() << ']';
            received_errors_++;
        }

        condition_.notify_one();
    }

    void join() { runner_.join(); }

    void run() {
        std::unique_lock<std::mutex> its_lock(mutex_);
        if (!condition_.wait_for(its_lock, std::chrono::seconds(5), [this] { return is_blocked_; })) {
            GTEST_FATAL_FAILURE_("Service didn't become available within 5s.");
        }

        VSOMEIP_INFO << "Running...";

        auto* its_routing = app_->routing_app_->routing_.get();
        ASSERT_TRUE(its_routing);

        // NOTE: these payloads *NEED* a magic cookie in front to be complete/parseable by the other side
        // which is how the test verifies anything at all
        vsomeip::byte_t its_good_payload_data[] = {0x12, 0x34, 0x84, 0x21, 0x00, 0x00, 0x00, 0x11, 0x13, 0x43, 0x00, 0x00, 0x01,
                                                   0x00, 0x00, 0x00, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};

        vsomeip::byte_t its_bad_payload_data[] = {0x12, 0x34, 0x84, 0x21, 0x00, 0x00, 0x01, 0x23, 0x13, 0x43, 0x00, 0x00, 0x01,
                                                  0x00, 0x00, 0x00, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};

        // magic cookies are only sent every 10s
        // to get around it, we hack deep, deep into the endpoint machinery to reset the timer after each message
        // TODO: FIXME, somehow, because disgusting does not begin to describe how terrible this is
        auto wait_and_reset_timer = [its_routing]() {
            for (size_t i = 0; i < 2000; ++i) {
                // NOTE: scope is intentional, need some care not to hold these internal locks for any amount of time
                {
                    std::scoped_lock its_lock{its_routing->get_endpoint_manager()->endpoint_mutex_};
                    for (const auto& its_address : its_routing->get_endpoint_manager()->client_endpoints_) {
                        for (const auto& its_port : its_address.second) {
                            for (const auto& its_reliability : its_port.second) {
                                for (const auto& its_partition : its_reliability.second) {
                                    if (auto endpoint = dynamic_cast<vsomeip::tcp_client_endpoint_impl*>(its_partition.second.get())) {
                                        std::scoped_lock its_inner_lock{endpoint->socket_mutex_};
                                        // was a message just sent? reset the timer!
                                        if (endpoint->last_cookie_sent_ > std::chrono::steady_clock::now() - 5s) {
                                            endpoint->last_cookie_sent_ = std::chrono::steady_clock::now() - 11s;
                                            VSOMEIP_INFO << "Reset cookie timer";
                                            return;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                std::this_thread::sleep_for(1ms);
            }

            GTEST_FATAL_FAILURE_("Could not reset cookie timer");
        };

        // Test sequence
        its_good_payload_data[11] = 0x01;
        auto* routing = static_cast<vsomeip::routing_manager*>(its_routing);
        routing->send(0x1343, its_good_payload_data, sizeof(its_good_payload_data), vsomeip_test::TEST_SERVICE_INSTANCE_ID, true);
        wait_and_reset_timer();
        its_bad_payload_data[11] = 0x02;
        routing->send(0x1343, its_bad_payload_data, sizeof(its_bad_payload_data), vsomeip_test::TEST_SERVICE_INSTANCE_ID, true);
        wait_and_reset_timer();
        its_good_payload_data[11] = 0x03;
        routing->send(0x1343, its_good_payload_data, sizeof(its_good_payload_data), vsomeip_test::TEST_SERVICE_INSTANCE_ID, true);
        wait_and_reset_timer();
        its_bad_payload_data[11] = 0x04;
        routing->send(0x1343, its_bad_payload_data, sizeof(its_bad_payload_data), vsomeip_test::TEST_SERVICE_INSTANCE_ID, true);
        wait_and_reset_timer();
        its_bad_payload_data[11] = 0x05;
        routing->send(0x1343, its_bad_payload_data, sizeof(its_bad_payload_data), vsomeip_test::TEST_SERVICE_INSTANCE_ID, true);
        wait_and_reset_timer();
        its_good_payload_data[11] = 0x06;
        routing->send(0x1343, its_good_payload_data, sizeof(its_good_payload_data), vsomeip_test::TEST_SERVICE_INSTANCE_ID, true);
        wait_and_reset_timer();
        its_good_payload_data[11] = 0x07;
        routing->send(0x1343, its_good_payload_data, sizeof(its_good_payload_data), vsomeip_test::TEST_SERVICE_INSTANCE_ID, true);
        wait_and_reset_timer();
        its_bad_payload_data[11] = 0x08;
        routing->send(0x1343, its_bad_payload_data, sizeof(its_bad_payload_data), vsomeip_test::TEST_SERVICE_INSTANCE_ID, true);
        wait_and_reset_timer();
        its_bad_payload_data[11] = 0x09;
        routing->send(0x1343, its_bad_payload_data, sizeof(its_bad_payload_data), vsomeip_test::TEST_SERVICE_INSTANCE_ID, true);
        wait_and_reset_timer();
        its_bad_payload_data[11] = 0x0A;
        routing->send(0x1343, its_bad_payload_data, sizeof(its_bad_payload_data), vsomeip_test::TEST_SERVICE_INSTANCE_ID, true);
        wait_and_reset_timer();
        its_good_payload_data[11] = 0x0B;
        routing->send(0x1343, its_good_payload_data, sizeof(its_good_payload_data), vsomeip_test::TEST_SERVICE_INSTANCE_ID, true);
        wait_and_reset_timer();
        its_good_payload_data[11] = 0x0C;
        routing->send(0x1343, its_good_payload_data, sizeof(its_good_payload_data), vsomeip_test::TEST_SERVICE_INSTANCE_ID, true);
        wait_and_reset_timer();
        its_good_payload_data[11] = 0x0D;
        routing->send(0x1343, its_good_payload_data, sizeof(its_good_payload_data), vsomeip_test::TEST_SERVICE_INSTANCE_ID, true);
        wait_and_reset_timer();
        its_bad_payload_data[11] = 0x0E;
        routing->send(0x1343, its_bad_payload_data, sizeof(its_bad_payload_data), vsomeip_test::TEST_SERVICE_INSTANCE_ID, true);
        wait_and_reset_timer();
        its_good_payload_data[11] = 0x0F;
        routing->send(0x1343, its_good_payload_data, sizeof(its_good_payload_data), vsomeip_test::TEST_SERVICE_INSTANCE_ID, true);
        wait_and_reset_timer();

        ASSERT_TRUE(condition_.wait_for(its_lock, 5s, [this] { return received_responses_ == 8 && received_errors_ == 7; }));

        stop();
    }

private:
    std::shared_ptr<vsomeip::application_impl> app_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool is_blocked_;
    std::atomic<std::uint32_t> received_responses_;
    std::atomic<std::uint32_t> received_errors_;
    std::thread runner_;
};

TEST(someip_magic_cookies_test, send_good_and_bad_messages) {
    magic_cookies_test_client its_client;
    its_client.init();
    its_client.start();
    its_client.join();
}

int main(int argc, char** argv) {
    return test_main(argc, argv);
}
