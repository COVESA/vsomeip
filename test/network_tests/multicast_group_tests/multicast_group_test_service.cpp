// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "multicast_group_test_globals.hpp"

using namespace multicast_group_test;

TEST(MulticastGroupTest, ServiceOffersTheService) {
    // Initialize the runtime
    auto runtime = vsomeip::runtime::get();
    ASSERT_TRUE(runtime) << "Should create a vsomeip runtime.";

    // Initialize the application
    auto service_application = runtime->create_application("service-sample");
    ASSERT_TRUE(service_application) << "Should create a vsomeip application.";
    ASSERT_TRUE(service_application->init()) << "Should initialize application.";

    // Offer the test service and client
    service_application->offer_service(SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION);
    service_application->offer_event(SERVICE_ID, INSTANCE_ID, EVENT_ID, {EVENTGROUP_ID}, vsomeip_v3::event_type_e::ET_EVENT);

    // subscribed_clients and has_no_subscribers is used to detect if both client
    // unsubscribed before sending all notifications, which indicates that the test failed
    int subscribed_clients = 0;
    std::atomic<bool> has_no_subscribers(false);

    // Register subscription handler to check if the clients are subscribed
    service_application->register_subscription_handler(
            SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID,
            [&](vsomeip::client_t _client, std::uint32_t, std::uint32_t, const std::string&, bool _subscribed) {
                VSOMEIP_INFO << "Client: 0x" << std::hex << _client << ((_subscribed) ? " subscribed" : " unsubscribed");
                _subscribed ? subscribed_clients++ : subscribed_clients--;
                if (subscribed_clients <= 0) {
                    has_no_subscribers = true;
                }
                return true;
            });

    // Start the vsomeip application
    std::thread worker_thread([service_application] { service_application->start(); });

    // Create payload
    vsomeip::byte_t its_data[2] = {0, 0};
    uint32_t its_size = 2;
    std::shared_ptr<vsomeip::payload> payload_;
    payload_ = vsomeip::runtime::get()->create_payload();

    for (uint16_t i = 1; i < 1000; i++) {

        // Check if there are still subscribers
        if (has_no_subscribers.load() == true) {
            ADD_FAILURE() << "Both clients unsubscribed before all notifications were sent!";
            break;
        }

        VSOMEIP_DEBUG << "Sending payload i=" << i;

        vsomeip_v3::bithelper::write_uint16_le(static_cast<uint16_t>(i), its_data);

        payload_->set_data(its_data, its_size);
        service_application->notify(SERVICE_ID, INSTANCE_ID, EVENT_ID, payload_);

        // Sleep for 10 milliseconds to send each notification in a 10 milliseconds interval
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    VSOMEIP_INFO << "Sent all notifications, ending test" << std::endl;

    // The final notifications corresponds to the max value
    its_data[0] = static_cast<uint8_t>(255);
    its_data[1] = static_cast<uint8_t>(255);
    payload_->set_data(its_data, its_size);
    service_application->notify(SERVICE_ID, INSTANCE_ID, EVENT_ID, payload_);

    // Sleep before stopping application
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    service_application->stop();

    // Clean up worker thread
    if (worker_thread.joinable()) {
        worker_thread.join();
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
