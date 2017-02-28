// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include <future>

#include <vsomeip/vsomeip.hpp>
#include "../../implementation/logging/include/logger.hpp"

class application_test_daemon {
public:
    application_test_daemon() :
            app_(vsomeip::runtime::get()->create_application("daemon")) {
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return;
        }
        std::promise<bool> its_promise;
        application_thread_ = std::thread([&](){
            its_promise.set_value(true);
            app_->start();
        });
        EXPECT_TRUE(its_promise.get_future().get());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
