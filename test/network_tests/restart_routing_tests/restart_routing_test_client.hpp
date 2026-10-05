// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>

#include "../someip_test_globals.hpp"
#include <common/vsomeip_app_utilities.hpp>
#include "restart_routing_test_globals.hpp"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>

enum sending_status { SEND_MESSAGES = 0x00, WAITING_TO_SEND_MESSAGES = 0x01 };

class restart_routing_test_client {
public:
    restart_routing_test_client();
    ~restart_routing_test_client();

    void init();
    void run();
    void on_state(vsomeip::state_type_e _state);
    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available);
    void on_message(const std::shared_ptr<vsomeip::message>& _response);
    bool wait_for_registration();
    void send_messages();
    bool wait_for_responses();

private:
    void stop();

    std::shared_ptr<vsomeip::application> app_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool is_available_;
    std::thread starter_;
    std::thread runner_;
    std::atomic<uint32_t> received_responses_;
    uint32_t app_id_;
    vsomeip::state_type_e registration_status_{vsomeip::state_type_e::ST_DEREGISTERED};
    sending_status sending_status_{sending_status::WAITING_TO_SEND_MESSAGES};
    bool all_responses_received_{false};
};
