// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>
#include "../../implementation/logging/include/logger.hpp"

class application_test_daemon {
public:
    application_test_daemon() :
            app_(vsomeip::runtime::get()->create_application("daemon")) {
        app_->init();
        application_thread_ = std::thread([&](){
            app_->start();
        });
        VSOMEIP_INFO << "Daemon starting";
    }

    ~application_test_daemon() {
        application_thread_.join();
    }

    void stop() {
        VSOMEIP_INFO << "Daemon stopping";
        app_->stop();
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    std::thread application_thread_;
};
