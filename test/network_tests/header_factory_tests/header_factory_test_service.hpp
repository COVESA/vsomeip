// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once
#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>

#include <mutex>

#include "../someip_test_globals.hpp"
#include <common/vsomeip_app_utilities.hpp>

class header_factory_test_service {
public:
    header_factory_test_service();
    void start();
    void on_message(const std::shared_ptr<vsomeip::message>& _request);

private:
    std::shared_ptr<vsomeip::application> app_;
    std::mutex sync_mtx_;
    std::uint32_t number_of_received_messages_;
};
