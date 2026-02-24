// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <cstdio>
#include <gtest/gtest.h>
#include <thread>
#include <vsomeip/vsomeip.hpp>

#include "common/test_main.hpp"
#include "invalid_event_test_globals.hpp"

using namespace invalid_event_test_globals;

TEST(invalid_event_test, service_is_offered) {
    // Create and initialize a vsomeip application to run the service.
    std::shared_ptr<vsomeip_v3::application> app = vsomeip_v3::runtime::get()->create_application();
    ASSERT_TRUE(app && app->init()) << "should create service application";

    // Run the vsomeip application in a dedicated thread.
    auto worker = std::thread([app] { app->start(); });

    // Offer the test service.
    app->offer_service(TEST_SERVICE_ID, TEST_INSTANCE_ID);
    app->offer_event(TEST_SERVICE_ID, TEST_INSTANCE_ID, TEST_VALID_EVENT_ID, {TEST_EVENTGROUP_ID}, vsomeip_v3::event_type_e::ET_EVENT,
                     std::chrono::milliseconds::zero(), false, true, nullptr, vsomeip_v3::reliability_type_e::RT_RELIABLE);

    // Wait for the termination signal.
    wait_for_signal();
    app->stop();

    // Wait for the app to stop.
    worker.join();
}

int main(int argc, char* argv[]) {
    block_signal(); // Blocks SIGUSR1 for the test.
    return test_main(argc, argv);
}
