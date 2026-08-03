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

class restart_routing_test_service {
public:
    restart_routing_test_service();
    ~restart_routing_test_service();
    void init();
    void offer();
    void stop_offer();
    void on_message(const std::shared_ptr<vsomeip::message>& _request);
    bool wait_for_messages();

private:
    std::shared_ptr<vsomeip::application> app_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool all_received_{false};
    std::map<uint16_t, uint32_t> received_counter_;
    uint32_t number_of_received_messages_{0};
    std::thread runner_;
    std::thread starter_;
};
