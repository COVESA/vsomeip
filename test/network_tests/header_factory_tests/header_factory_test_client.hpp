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

class header_factory_test_client {
public:
    header_factory_test_client();
    void start();
    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available);
    void on_message(const std::shared_ptr<vsomeip::message>& _response);

private:
    std::shared_ptr<vsomeip::application> app_;
    std::shared_ptr<vsomeip::message> request_;
    std::mutex sync_mtx_;
    std::uint32_t number_of_messages_to_send_;
    std::uint32_t number_of_acknowledged_messages_;
};
