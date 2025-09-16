// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <atomic>
#include <cstdint>
#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vsomeip/vsomeip.hpp>

#include "someip_test_globals.hpp"

// Eventgroup ID of the test service event.
constexpr vsomeip::eventgroup_t TEST_SERVICE_EVENTGROUP_ID = 1;

TEST(debounce_filter_tests, client_receives_notifications) {
    // Create and initialize the vsomeip application.
    auto app = vsomeip::runtime::get()->create_application();
    ASSERT_TRUE(app) << "should create a vsomeip application";
    ASSERT_TRUE(app->init()) << "should initialize the vsomeip application";

    // Start the vsomeip application.
    auto worker = std::thread([&] { app->start(); });

    // Register an availability handler for the test service.
    auto available = std::promise<bool>();
    app->register_availability_handler(vsomeip_test::TEST_SERVICE_SERVICE_ID,
                                       vsomeip_test::TEST_SERVICE_INSTANCE_ID,
                                       [&](vsomeip::service_t /* service */,
                                           vsomeip::instance_t /* instance */, bool is_available) {
                                           if (is_available) {
                                               available.set_value(true);
                                           }
                                       });

    // Request the test service.
    app->request_service(vsomeip_test::TEST_SERVICE_SERVICE_ID,
                         vsomeip_test::TEST_SERVICE_INSTANCE_ID);
    app->request_event(vsomeip_test::TEST_SERVICE_SERVICE_ID,
                       vsomeip_test::TEST_SERVICE_INSTANCE_ID, vsomeip_test::TEST_SERVICE_METHOD_ID,
                       {TEST_SERVICE_EVENTGROUP_ID});

    VSOMEIP_INFO << "debounce_epsilon_test_client: Waiting for test service availability.";

    // Wait for the service to become available.
    available.get_future().wait();

    // Register a handler for test service notifications.
    auto notified = std::promise<bool>();
    app->register_message_handler(
            vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID,
            vsomeip_test::TEST_SERVICE_METHOD_ID,
            [&](const std::shared_ptr<vsomeip::message>& notification) {
                // Check the contents of the notification.
                auto value = notification->get_payload()->get_data()[0];
                VSOMEIP_DEBUG << "debounce_epsilon_test_client: value=" << static_cast<int>(value);
                EXPECT_EQ(value % 10, 0)
                        << "only values divisible by 10 should pass the epsilon func.";

                // Check if we received enough notifications to be able to confirm that the debounce
                // is working as intended.
                static std::atomic_uint16_t notification_count {0};
                constexpr std::uint16_t EXPECTED_NOTIFICATION_COUNT {2};
                if (++notification_count == EXPECTED_NOTIFICATION_COUNT) {
                    notified.set_value(true);
                }
            });

    // Subscribe to the eventgroup.
    app->subscribe(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID,
                   TEST_SERVICE_EVENTGROUP_ID);

    VSOMEIP_INFO << "debounce_epsilon_test_client: Waiting for test service notifications.";

    // Wait to be notified.
    notified.get_future().wait();

    // Register a handler for the shutdown response.
    auto shutdown = std::promise<bool>();
    app->register_message_handler(vsomeip_test::TEST_SERVICE_SERVICE_ID,
                                  vsomeip_test::TEST_SERVICE_INSTANCE_ID,
                                  vsomeip_test::TEST_SERVICE_METHOD_ID_SHUTDOWN,
                                  [&](const std::shared_ptr<vsomeip::message>& /* response */) {
                                      shutdown.set_value(true);
                                  });

    // Send a shutdown request to the test service.
    auto request = vsomeip::runtime::get()->create_request();
    request->set_service(vsomeip_test::TEST_SERVICE_SERVICE_ID);
    request->set_instance(vsomeip_test::TEST_SERVICE_INSTANCE_ID);
    request->set_method(vsomeip_test::TEST_SERVICE_METHOD_ID_SHUTDOWN);
    app->send(request);

    VSOMEIP_INFO << "debounce_epsilon_test_client: Waiting for shutdown response.";

    // Wait for the shutdown response.
    shutdown.get_future().wait();

    // Stop vsomeip.
    app->stop();

    // Rejoin the worker thread.
    worker.join();

    VSOMEIP_INFO << "debounce_epsilon_test_client: Done.";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
