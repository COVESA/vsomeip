// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <cstdint>
#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vsomeip/vsomeip.hpp>

#include "someip_test_globals.hpp"
#include "vsomeip/internal/logger.hpp"

// Eventgroup ID of the test service event.
constexpr vsomeip::eventgroup_t TEST_SERVICE_EVENTGROUP_ID = 1;

// Whether the value of the first byte of the payload is divisible by 10.
//
// This is intended to be used as an epsilon change function to debounce outgoing events.
bool is_payload_divisible_by_10(const std::shared_ptr<vsomeip::payload>& /* old */, const std::shared_ptr<vsomeip::payload>& payload) {
    auto value = payload->get_data()[0];
    return value % 10 == 0;
}

TEST(debounce_filter_tests, server_sends_notifications) {
    // Create and initialize the vsomeip application.
    auto app = vsomeip::runtime::get()->create_application();
    ASSERT_TRUE(app) << "should create a vsomeip application";
    ASSERT_TRUE(app->init()) << "should initialize the vsomeip application";

    // Start the vsomeip application.
    auto worker = std::thread([&] { app->start(); });

    // Register a handler for the shutdown request.
    auto shutdown = std::promise<bool>();
    app->register_message_handler(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID,
                                  vsomeip_test::TEST_SERVICE_METHOD_ID_SHUTDOWN, [&](const std::shared_ptr<vsomeip::message>& request) {
                                      // Send the response back to the client.
                                      auto response = vsomeip::runtime::get()->create_response(request);
                                      app->send(response);

                                      // Signal the service to shutdown.
                                      shutdown.set_value(true);
                                  });

    // Offer the test service.
    app->offer_service(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID);
    app->offer_event(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID, vsomeip_test::TEST_SERVICE_METHOD_ID,
                     {TEST_SERVICE_EVENTGROUP_ID}, vsomeip::event_type_e::ET_EVENT, std::chrono::milliseconds::zero(), false, true,
                     &is_payload_divisible_by_10 // Epsilon change function.
    );

    VSOMEIP_INFO << "debounce_epsilon_test_service: Waiting for shutdown request.";

    // Cyclicly send a notification with payloads of increasing value until we receive a shutdown
    // request from the client.
    std::uint8_t payload = 1;
    auto future = shutdown.get_future();
    for (;;) {
        // Update and send notification.
        app->notify(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID, vsomeip_test::TEST_SERVICE_METHOD_ID,
                    vsomeip::runtime::get()->create_payload({++payload}));

        // Wait and see if the client requested the shutdown.
        auto result = future.wait_for(std::chrono::milliseconds(10));
        if (result == std::future_status::ready) {
            break;
        }
    }

    // Give vsomeip enough time to process the shutdown response.
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Stop vsomeip.
    app->stop();

    // Rejoin the worker thread.
    worker.join();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
