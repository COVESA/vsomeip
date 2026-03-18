// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "connection_timeout_sub_ack_test_globals.hpp"

using namespace connection_timeout_sub_ack_test;

TEST(ConnectionTimeoutSubAckTest, ClientSubscribesToService) {

    // Variables to hold the subscribe status
    std::atomic<bool> client_subscribed(false);
    bool client_resubscribed = false;
    std::mutex client_resubscribed_mutex;
    std::condition_variable client_resubscribed_cv;

    // Initialize the runtime
    auto runtime = vsomeip::runtime::get();
    ASSERT_TRUE(runtime) << "Should create a vsomeip runtime.";

    // Initialize the application
    auto application = runtime->create_application("client-app");
    ASSERT_TRUE(application) << "Should create a vsomeip application.";
    ASSERT_TRUE(application->init()) << "Should initialize application.";

    // Request the test service
    application->request_service(SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION);
    application->request_event(SERVICE_ID, INSTANCE_ID, EVENT_ID, {EVENTGROUP_ID});
    application->subscribe(SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID, MAJOR_VERSION);

    // When the service becomes unavailable after the time out, signal the script to restore comms
    application->register_availability_handler(
            SERVICE_ID, INSTANCE_ID,
            [&client_subscribed](vsomeip::service_t /* service */, vsomeip::instance_t /* instance */, bool is_available) {
                if (!is_available && client_subscribed) {
                    std::filesystem::path filename = std::filesystem::current_path() / "service_unavailable.flag";
                    std::ofstream file(filename);
                    ASSERT_TRUE(file) << "Failed to create service unavailable file!";
                }
            });

    // When the client subscribes, signal the script to stop comms
    // When the client resusbcribes, finish the test
    application->register_subscription_status_handler(
            SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID, EVENT_ID,
            [&client_subscribed, &client_resubscribed, &client_resubscribed_mutex, &client_resubscribed_cv](
                    const vsomeip::service_t, vsomeip::instance_t, vsomeip::eventgroup_t, vsomeip::event_t, const uint16_t) {
                if (client_subscribed == false) {
                    client_subscribed = true;
                    std::filesystem::path filename = std::filesystem::current_path() / "client_subscribed.flag";
                    std::ofstream file(filename);
                    ASSERT_TRUE(file) << "Failed to create client subscribed file!";
                } else {
                    std::unique_lock<std::mutex> its_lock(client_resubscribed_mutex);
                    client_resubscribed = true;
                    client_resubscribed_cv.notify_one();
                }
            },
            false);

    // Start the vsomeip application
    std::thread start_thread([&application] { application->start(); });

    // Wait until the client resubscribes to the service
    {
        std::unique_lock<std::mutex> its_lock(client_resubscribed_mutex);
        client_resubscribed_cv.wait_for(its_lock, std::chrono::seconds(30), [&client_resubscribed] { return client_resubscribed == true; });
    }

    // Stop the application after the test is done
    application->stop();

    // Clean up worker thread
    if (start_thread.joinable()) {
        start_thread.join();
    }

    // Once the test is done, create the file to signal it to the server
    std::filesystem::path filename = std::filesystem::current_path() / "shutdown_test.flag";
    std::ofstream file(filename);
    ASSERT_TRUE(file) << "Failed to create shutdown file!";
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    return test_main(argc, argv, std::chrono::seconds(30));
}
#endif
