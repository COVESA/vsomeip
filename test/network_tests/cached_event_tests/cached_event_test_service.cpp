// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <future>
#include <gtest/gtest.h>
#include <thread>
#include <vsomeip/vsomeip.hpp>

#include "cached_event_test_globals.hpp"
#include "vsomeip/internal/logger.hpp"

using namespace cached_event_test;

TEST(cached_event_test, service_is_offered) {
    // Create the service application.
    auto app = vsomeip::runtime::get()->create_application();
    ASSERT_TRUE(app) << "should create the service application";
    ASSERT_TRUE(app->init()) << "should initialize the service application";

    // Start the service.
    auto worker = std::thread([&] { app->start(); });

    // Prepare to be notified of the special service availability. This is used as a signal for us
    // to start offering the test service.
    auto available = std::promise<bool>();
    app->register_availability_handler(SPECIAL_SERVICE_ID, INSTANCE_ID,
                                       [&](vsomeip::service_t /* service */, vsomeip::instance_t /* instance */, const bool is_available) {
                                           if (!is_available) {
                                               return;
                                           }
                                           VSOMEIP_INFO << "[TEST]: Special service is available.";
                                           available.set_value(true);
                                       });

    // Request the special service.
    app->request_service(SPECIAL_SERVICE_ID, INSTANCE_ID);

    // Wait for the special service to become available.
    available.get_future().wait();

    // Prepare to receive a shutdown command when the test is done.
    auto shutdown = std::promise<bool>();
    app->register_message_handler(SERVICE_ID, INSTANCE_ID, SHUTDOWN_ID, [&](const std::shared_ptr<vsomeip::message>& /* message */) {
        VSOMEIP_INFO << "[TEST]: Received a shutdown request.";
        shutdown.set_value(true);
    });

    // Offer the test service and set the event payload.
    app->offer_service(SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION);
    app->offer_event(SERVICE_ID, INSTANCE_ID, EVENT_ID, {EVENTGROUP_ID}, vsomeip::event_type_e::ET_FIELD);
    app->notify(SERVICE_ID, INSTANCE_ID, EVENT_ID, vsomeip::runtime::get()->create_payload({0x01}));

    // Wait for the shutdown request.
    shutdown.get_future().wait();

    // Stop the service application and clean up the worker thread.
    app->stop();
    worker.join();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
