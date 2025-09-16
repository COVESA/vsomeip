// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "hostname_test_globals.hpp"

using namespace hostname_test;

TEST(HostnameTest, ServiceOffersService) {
    // Initialize the runtime
    auto runtime = vsomeip::runtime::get();
    ASSERT_TRUE(runtime) << "Should create a vsomeip runtime.";

    // Initialize the application
    auto application = runtime->create_application("service-sample");
    ASSERT_TRUE(application) << "Should create a vsomeip application.";
    ASSERT_TRUE(application->init()) << "Should initialize application.";

    // Create future and promise to know when client subscribes
    std::promise<bool> is_subscribed_prom;
    std::future<bool> is_subscribed_fut = is_subscribed_prom.get_future();

    // Create hostname variables
    std::string _client_hostname;
    char hostname[256];

    // Check hostname from service side
    gethostname(hostname, sizeof(hostname));
    EXPECT_NE(hostname, "");

    // Register subscription handler
    application->register_subscription_handler(
            SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID,
            [&](vsomeip::client_t _client, std::uint32_t, std::uint32_t, const std::string& _env, bool _subscribed) {
                VSOMEIP_INFO << __func__ << ": client: 0x" << std::hex << _client << ((_subscribed) ? " subscribed" : " unsubscribed")
                             << ", client hostname:" << _env;
                if (_subscribed) {
                    _client_hostname = _env;
                    is_subscribed_prom.set_value(true);
                }
                return true;
            });

    // Offer the test service
    application->offer_service(SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION);
    application->offer_event(SERVICE_ID, INSTANCE_ID, EVENT_ID, {EVENTGROUP_ID}, vsomeip_v3::event_type_e::ET_EVENT);

    // Start the vsomeip application
    std::thread worker_thread([&application] { application->start(); });

    // Wait for client to subscribe before stopping application
    is_subscribed_fut.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    application->stop();

    // Check if hostname is the same for client and service
    EXPECT_EQ(_client_hostname, hostname);

    // Clean up worker thread
    if (worker_thread.joinable()) {
        worker_thread.join();
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
