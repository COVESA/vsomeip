// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <mutex>
#include <thread>

#include <common/vsomeip_app_utilities.hpp>
#include <gtest/gtest.h>
#include <vsomeip/vsomeip.hpp>
#include "reuse_client_id_test_globals.hpp"

class reuse_client_id_test_client {
public:
    reuse_client_id_test_client(const char* app_name_, uint32_t _app_id);

    void on_state(vsomeip::state_type_e _state);
    void run();

private:
    std::shared_ptr<vsomeip::application> app_;
    uint32_t app_id_;
    std::atomic<bool> is_registered;
    bool restarted{false};
    reuse_client_id::reuse_client_id_test_interprocess_sync* ip_sync{nullptr};
    std::atomic<bool> stopping{false};
};
