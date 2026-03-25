// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>

#include <thread>

class npdu_test_rmd {

public:
    npdu_test_rmd();
    void init();
    void start();
    void stop();
    void on_state(vsomeip::state_type_e _state);
    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available);
    void on_message_shutdown(const std::shared_ptr<vsomeip::message>& _request);

private:
    std::shared_ptr<vsomeip::application> app_;
};
