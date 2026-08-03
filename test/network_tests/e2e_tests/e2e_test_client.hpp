// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>

#include "../someip_test_globals.hpp"
#include <common/vsomeip_app_utilities.hpp>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

class e2e_test_client {
public:
    e2e_test_client();
    bool init();
    void start();
    void stop();

    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available);
    void on_message(const std::shared_ptr<vsomeip::message>& _response);

    void initialize_e2e_provider();
    void calculate_next_pf1_expected_payload();

    void run();
    void join_sender_thread();

private:
    void shutdown_service();

    std::shared_ptr<vsomeip::application> app_;

    std::mutex mutex_;
    std::condition_variable condition_;
    bool is_available_;

    std::thread sender_;

    std::atomic<uint32_t> received_responses_;
    std::atomic<uint32_t> received_allowed_events_;
};
