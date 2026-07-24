// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>

#include "memory_test_common.hpp"
#include <common/vsomeip_app_utilities.hpp>

class memory_test_client : public vsomeip_utilities::base_vsip_app {
public:
    memory_test_client(const char* app_name_, std::map<vsomeip::event_t, int> map_events_);
    void send_request(std::atomic<bool>& stop_checking_);
    std::uint64_t baseline_rss() const { return baseline_rss_; }

    ~memory_test_client();

private:
    std::condition_variable condition_availability;
    std::mutex availability_mutex;
    std::mutex event_counter_mutex;
    bool availability{false};
    std::uint64_t received_messages_counter{0};
    std::uint64_t baseline_rss_{0};
    std::map<vsomeip::event_t, int> map_events;
    std::chrono::time_point<std::chrono::system_clock> sec;
    void on_availability(vsomeip::service_t service_, vsomeip::instance_t instance_, bool is_available_);
    void on_message(const std::shared_ptr<vsomeip::message>& message_);
    void send_ack(std::uint64_t received_count_);
    void stop_service();
    void unsubscribe_all();
};
