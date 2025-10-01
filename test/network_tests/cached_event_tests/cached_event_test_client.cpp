// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vsomeip/vsomeip.hpp>

#include "cached_event_test_globals.hpp"
#include "vsomeip/internal/logger.hpp"

using namespace cached_event_test;

TEST(cached_event_test, clients_are_notified) {
    // Create the first client application. This is the one that will subscribe to the event first
    // in order to cache the event.
    auto client_1 = vsomeip::runtime::get()->create_application("client-sample");
    ASSERT_TRUE(client_1) << "should create the first client";
    ASSERT_TRUE(client_1->init()) << "should initialize the first client";

    // Start the client application in a separate thread.
    auto client_1_worker = std::thread([&] { client_1->start(); });

    // Prepare to be notified of the test service availability.
    auto available = std::promise<bool>();
    client_1->register_availability_handler(
            SERVICE_ID, INSTANCE_ID, [&](vsomeip::service_t /* service */, vsomeip::instance_t /* instance */, const bool is_available) {
                if (!is_available) {
                    return;
                }
                VSOMEIP_INFO << "[TEST]: Test service is available.";
                available.set_value(true);
            });

    // Request the service before it is offered. While this is not required in order to cache the
    // event, we need it to check an edge-case in the cache implementation where the interface
    // version is not properly updated.
    client_1->request_service(SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION);
    client_1->request_event(SERVICE_ID, INSTANCE_ID, EVENT_ID, {EVENTGROUP_ID}, vsomeip::event_type_e::ET_FIELD);

    // Notify the service provider to start offering the test service.
    client_1->offer_service(SPECIAL_SERVICE_ID, INSTANCE_ID);

    // Wait for the test service to become available.
    available.get_future().wait();

    // Prepare to receive the test service notification.
    auto client_1_notified = std::promise<bool>();
    client_1->register_message_handler(SERVICE_ID, INSTANCE_ID, EVENT_ID, [&](const std::shared_ptr<vsomeip::message>& message) {
        VSOMEIP_INFO << "[TEST]: Client 1 received a notification.";
        EXPECT_EQ(message->get_service(), SERVICE_ID);
        EXPECT_EQ(message->get_instance(), INSTANCE_ID);
        EXPECT_EQ(message->get_method(), EVENT_ID);
        EXPECT_EQ(message->get_interface_version(), MAJOR_VERSION);
        EXPECT_EQ(message->get_payload()->get_data()[0], vsomeip::runtime::get()->create_payload({0x01})->get_data()[0]);
        client_1_notified.set_value(true);
    });

    // Subscribe to the event.
    client_1->subscribe(SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID, MAJOR_VERSION, EVENT_ID);

    // Wait to be notified.
    client_1_notified.get_future().wait();

    // Create the second client application. This one should receive the cached event when it
    // subscribes.
    auto client_2 = vsomeip::runtime::get()->create_application("second-client-sample");
    ASSERT_TRUE(client_2) << "should create the second client";
    ASSERT_TRUE(client_2->init()) << "should initialize the second client";

    // Start the client application in a separate thread.
    auto client_2_worker = std::thread([&] { client_2->start(); });

    // Prepare to receive the cached test service notification.
    auto client_2_notified = std::promise<bool>();
    client_2->register_message_handler(SERVICE_ID, INSTANCE_ID, EVENT_ID, [&](const std::shared_ptr<vsomeip::message>& message) {
        VSOMEIP_INFO << "[TEST]: Client 2 received a notification.";
        EXPECT_EQ(message->get_service(), SERVICE_ID);
        EXPECT_EQ(message->get_instance(), INSTANCE_ID);
        EXPECT_EQ(message->get_method(), EVENT_ID);
        EXPECT_EQ(message->get_interface_version(), MAJOR_VERSION);
        EXPECT_EQ(message->get_payload()->get_data()[0], vsomeip::runtime::get()->create_payload({0x01})->get_data()[0]);
        client_2_notified.set_value(true);
    });

    // Request the test service and subscribe to the event.
    client_2->request_service(SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION);
    client_2->request_event(SERVICE_ID, INSTANCE_ID, EVENT_ID, {EVENTGROUP_ID}, vsomeip::event_type_e::ET_FIELD);
    client_2->subscribe(SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID, MAJOR_VERSION, EVENT_ID);

    // Wait to be notified.
    client_2_notified.get_future().wait();

    // Send a shutdown request to the service.
    auto shutdown = vsomeip::runtime::get()->create_request();
    shutdown->set_service(SERVICE_ID);
    shutdown->set_instance(INSTANCE_ID);
    shutdown->set_interface_version(MAJOR_VERSION);
    shutdown->set_method(SHUTDOWN_ID);
    client_1->send(shutdown);

    // Wait for the shutdown to be processed.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop the clients and clean up the worker threads.
    client_2->stop();
    client_2_worker.join();
    client_1->stop();
    client_1_worker.join();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
