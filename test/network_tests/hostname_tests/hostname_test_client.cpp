// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "hostname_test_globals.hpp"

using namespace hostname_test;

TEST(HostnameTest, ClientSubscribesToService) {
    // Create future and promise to know when client subscribes
    std::promise<bool> is_subscribed_prom;
    std::future<bool> is_subscribed_fut = is_subscribed_prom.get_future();

    // Initialize the runtime
    auto runtime = vsomeip::runtime::get();
    ASSERT_TRUE(runtime) << "Should create a vsomeip runtime.";

    // Initialize the application
    auto application = runtime->create_application("client-sample");
    ASSERT_TRUE(application) << "Should create a vsomeip application.";
    ASSERT_TRUE(application->init()) << "Should initialize application.";

    // Subscribe to the test service when it becomes available
    application->register_availability_handler(
            SERVICE_ID, INSTANCE_ID,
            [&](vsomeip::service_t, vsomeip::instance_t, bool is_available) {
                if (is_available) {
                    application->subscribe(SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID, MAJOR_VERSION);
                }
            },
            MAJOR_VERSION, MINOR_VERSION);

    // Sets is_subscribed_prom when subscription is done
    application->register_subscription_status_handler(
            SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID, EVENT_ID,
            [&is_subscribed_prom](const vsomeip::service_t, vsomeip::instance_t, vsomeip::eventgroup_t, vsomeip::event_t, const uint16_t) {
                VSOMEIP_INFO << "Registered to the event!";
                is_subscribed_prom.set_value(true);
            },
            false);

    // Request the test service
    application->request_service(SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION);
    application->request_event(SERVICE_ID, INSTANCE_ID, EVENT_ID, {EVENTGROUP_ID});

    // Start the vsomeip application
    std::thread worker_thread([&application] { application->start(); });

    // Wait for subscribe to be completed
    is_subscribed_fut.wait();
    application->stop();

    // Clean up worker thread
    if (worker_thread.joinable()) {
        worker_thread.join();
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
