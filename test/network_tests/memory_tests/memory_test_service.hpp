// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>
#include <common/vsomeip_app_utilities.hpp>

#include "memory_test_common.hpp"

class memory_test_service : public vsomeip_utilities::base_vsip_app {
public:
    memory_test_service(const char* app_name_);
    void setup_app(const std::function<void(void)> executionHandler_);
    void message_sender(std::atomic<bool>& stop_checking_);

private:
    std::condition_variable condition_wait_start;
    std::condition_variable condition_wait_stop;
    std::mutex start_mutex;
    std::mutex stop_mutex;
    bool received_message{false};

    // Number of notifications the client has confirmed receiving (cumulative
    // high-water mark reported via MEMORY_ACK_METHOD).
    std::atomic<uint64_t> acked_count_{0};

    void on_start(const std::shared_ptr<vsomeip::message> /*&_message*/);
    void on_stop(const std::shared_ptr<vsomeip::message> /*&_message*/);
    void on_ack(const std::shared_ptr<vsomeip::message>& _message);
    // Blocks until fewer than FLOW_CONTROL_WINDOW messages are outstanding, or
    // until the acknowledged count stalls (consumer gone).
    void wait_for_flow_control(uint64_t sent_);
};
