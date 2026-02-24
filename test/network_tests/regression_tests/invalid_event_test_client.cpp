// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <future>
#include <gtest/gtest.h>
#include <ios>
#include <iostream>
#include <ostream>
#include <thread>
#include <vsomeip/vsomeip.hpp>

#include "common/test_main.hpp"
#include "invalid_event_test_globals.hpp"

using namespace invalid_event_test_globals;

// Whether the client should register the invalid event.
static std::atomic_bool is_invalid{false};

TEST(invalid_event_test, subscription_succeeds) {
    // vsomeip application that subscribes to the service.
    std::shared_ptr<vsomeip_v3::application> app_ = vsomeip_v3::runtime::get()->create_application();
    ASSERT_TRUE(app_ && app_->init()) << "should create service application";

    // Run the vsomeip application in a dedicated thread.
    auto worker = std::thread([app_] { app_->start(); });

    // Request the test service.
    app_->request_service(TEST_SERVICE_ID, TEST_INSTANCE_ID);
    app_->request_event(TEST_SERVICE_ID, TEST_INSTANCE_ID, TEST_VALID_EVENT_ID, {TEST_EVENTGROUP_ID}, vsomeip_v3::event_type_e::ET_EVENT,
                        vsomeip_v3::reliability_type_e::RT_RELIABLE);

    // Request the invalid event.
    if (is_invalid.load()) {
        app_->request_event(TEST_SERVICE_ID, TEST_INSTANCE_ID, TEST_INVALID_EVENT_ID, {TEST_EVENTGROUP_ID},
                            vsomeip_v3::event_type_e::ET_EVENT, vsomeip_v3::reliability_type_e::RT_BOTH); // <-- Wrong reliability!
        // Let the master know that the invalid event is registered.
        app_->offer_service(ON_REGISTERED_SERVICE_ID, ON_REGISTERED_INSTANCE_ID);
    }

    // Prepare to receive subscription status updates.
    auto subscribed = std::promise<bool>();
    app_->register_subscription_status_handler(TEST_SERVICE_ID, TEST_INSTANCE_ID, TEST_EVENTGROUP_ID, TEST_VALID_EVENT_ID,
                                               [&](const vsomeip_v3::service_t, const vsomeip_v3::instance_t,
                                                   const vsomeip_v3::eventgroup_t, const vsomeip_v3::event_t, const uint16_t _status) {
                                                   constexpr uint8_t SUBSCRIPTION_OK = 0x00;
                                                   // constexpr uint8_t SUBSCRIPTION_NOK = 0x07; <-- For reference.
                                                   try {
                                                       subscribed.set_value(_status == SUBSCRIPTION_OK);
                                                   } catch (...) { }
                                               });

    // Subscribe to the event group.
    app_->subscribe(TEST_SERVICE_ID, TEST_INSTANCE_ID, TEST_EVENTGROUP_ID);

    // Check if the subscription was successful.
    auto subscribed_fut = subscribed.get_future();
    subscribed_fut.wait();
    ASSERT_TRUE(subscribed_fut.get()) << "should subscribe to the event group";

    // Wait for the termination signal.
    if (is_invalid.load()) {
        wait_for_signal();
    }
    // Stop the app.
    app_->stop();

    // Wait for the app to stop.
    worker.join();
}

int main(int argc, char* argv[]) {
    // Block SIGUSR1 for the test.
    block_signal();

    // Check if we are the invalid client.
    for (int i = 0; i < argc; i++) {
        if (std::strcmp(argv[i], "--invalid") == 0) {
            is_invalid.store(true);
        }
    }
    std::cout << "TEST: is_invalid=" << std::boolalpha << is_invalid.load() << std::endl;

    // Run the test.
    return test_main(argc, argv);
}
